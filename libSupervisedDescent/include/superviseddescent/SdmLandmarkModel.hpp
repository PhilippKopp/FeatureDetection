/*
 * SdmLandmarkModel.hpp
 *
 *  Created on: 02.02.2014
 *      Author: Patrik Huber
 */

#pragma once

#ifndef SDMLANDMARKMODEL_HPP_
#define SDMLANDMARKMODEL_HPP_

#include "superviseddescent/DescriptorExtractor.hpp"
#include "superviseddescent/utils.hpp"
#include "imageio/LandmarkCollection.hpp"

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/nonfree/nonfree.hpp"
#ifdef WIN32
	#define BOOST_ALL_DYN_LINK	// Link against the dynamic boost lib. Seems to be necessary because we use /MD, i.e. link to the dynamic CRT.
	#define BOOST_ALL_NO_LIB	// Don't use the automatic library linking by boost with VS2010 (#pragma ...). Instead, we specify everything in cmake.
#endif
#include "boost/filesystem/path.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/lexical_cast.hpp"

extern "C" {
	#include "superviseddescent/hog.h"
}

using cv::Mat;
using cv::Scalar;
using std::vector;
using std::string;
using boost::lexical_cast;

namespace superviseddescent {

/**
 * A class representing a landmark model trained
 * with the supervised descent method.
 *
 * Todo: Write something about how the landmarks are represented?
 */
class SdmLandmarkModel  {

public:
	// should consider deleting this
	SdmLandmarkModel();

	/**
	* Constructs a new SdmModel.
	*
	* @param[in] a b
	*/
	SdmLandmarkModel(cv::Mat meanLandmarks, std::vector<std::string> landmarkIdentifier, std::vector<cv::Mat> regressorData, std::vector<std::shared_ptr<DescriptorExtractor>> descriptorExtractors, std::vector<std::string> descriptorTypes);

	struct HogParameter // Todo remove?
	{
		int cellSize;
		int numBins;
	};

	int getNumLandmarks() const;

	int getNumCascadeSteps() const;

	// Todo remove?
	HogParameter getHogParameters(int cascadeLevel) {
		return hogParameters[cascadeLevel];
	}

	/**
	* Returns the mean of the shape- and color model
	* as a Mesh.
	*
	* @return The mean of the model.
	*/
	// returns a copy. col-vec. ext always col, internal row
	cv::Mat getMeanShape() const;

	// returns  a header that points to the original data
	cv::Mat getRegressorData(int cascadeLevel);

	std::shared_ptr<DescriptorExtractor> getDescriptorExtractor(int cascadeLevel);
	
	std::string getDescriptorType(int cascadeLevel);

	//std::vector<cv::Point2f> getLandmarksAsPoints(cv::Mat or vector<float> alphas or empty(=mean));
	std::vector<cv::Point2f> getMeanAsPoints() const;

	/**
	 * Get the model's points as a LandmarkCollection.
	 * If nothing passed, return the mean.
	 *
	 * @param[in] modelInstance Todo.
	 * @return Todo.
	 */
	imageio::LandmarkCollection getAsLandmarks(cv::Mat modelInstance = cv::Mat()) const;

	cv::Point2f getLandmarkAsPoint(std::string landmarkIdentifier, cv::Mat modelInstance=cv::Mat()) const;

	void save(boost::filesystem::path filename, std::string comment="");

	/**
	* Load a SdmLandmarkModel model TODO a property tree node in a config file.
	* The function uses the file extension to determine which load
	* Throws a std::... ....
	*
	* @param[in] configTree A node of a ptree.
	* @return A morphable model.
	*/
	static SdmLandmarkModel load(boost::filesystem::path filename);

private:
	cv::Mat meanLandmarks; // 1 x numLandmarks*2. First all the x-coordinates, then all the y-coordinates.
	std::vector<std::string> landmarkIdentifiers; //
	std::vector<cv::Mat> regressorData; // Holds the training data, one cv::Mat for each cascade level. Every Mat is (numFeatureDim+1) x numLandmarks*2 (for x & y)

	std::vector<HogParameter> hogParameters;
	std::vector<std::shared_ptr<DescriptorExtractor>> descriptorExtractors;
	std::vector<std::string> descriptorTypes; //

};


/*
Some notes:
- The current model ('SDM_Model_HOG_Zhenhua_11012014.txt') uses roughly 1/10 of
the training data of the original model from the paper, and has no expressions

- One problem: Running the optimization several times doesn't result in better
performance. Two possible reasons:
* In the training, what we train is the step from the mean to the groundtruth.
So we only train a big step.
- Actually, that means that it's very important to get the rigid alignment
right to get the first update-step right?
* The update-step for one landmark is dependent on the other landmarks

Test: To calculate the face-box (Zhenhua): Take all 68 LMs; Take the min/max x and y
for the face-box. (so the face-box is quite small)
*/

// I think most (all?) of these functions could be free functions, we don't need a class? Do we have a state?
class SdmLandmarkModelFitting
{
public:
	SdmLandmarkModelFitting(SdmLandmarkModel model)/* : model(model)*/ {
		this->model = model;
	};

	// out: aligned modelShape
	// in: Rect, ocv with tl x, tl y, w, h (?) and calcs center
	// directly modifies modelShape
	// could move to parent-class
	// assumes mean -0.5, 0.5 and just places inside FB
	cv::Mat alignRigid(cv::Mat modelShape, cv::Rect faceBox) const {
		// we assume we get passed a col-vec. For convenience, we keep it.
		if (modelShape.cols != 1) {
			throw std::runtime_error("The supplied model shape does not have one column (i.e. it doesn't seem to be a column-vector).");
			// We could also check if it's a row-vector and if yes, transpose.
		}
		Mat xCoords = modelShape.rowRange(0, modelShape.rows / 2);
		Mat yCoords = modelShape.rowRange(modelShape.rows / 2, modelShape.rows);		
		// b) Align the model to the current face-box. (rigid, only centering of the mean). x_0
		// Initial estimate x_0: Center the mean face at the [-0.5, 0.5] x [-0.5, 0.5] square (assuming the face-box is that square)
		// More precise: Take the mean as it is (assume it is in a space [-0.5, 0.5] x [-0.5, 0.5]), and just place it in the face-box as
		// if the box is [-0.5, 0.5] x [-0.5, 0.5]. (i.e. the mean coordinates get upscaled)
		xCoords = (xCoords + 0.5f) * faceBox.width + faceBox.x;
		yCoords = (yCoords + 0.5f) * faceBox.height + faceBox.y;

		/*
		// Old algorithm Zhenhua:
		// scale the model:
		double minX, maxX, minY, maxY;
		cv::minMaxLoc(xCoords, &minX, &maxX);
		cv::minMaxLoc(yCoords, &minY, &maxY);
		float faceboxScaleFactor = 1.25f; // 1.25f: value of Zhenhua Matlab FD. Mine: 1.35f
		float modelWidth = maxX - minX;
		float modelHeight = maxY - minY;
		// scale it:
		modelShape = modelShape * (faceBox.width / modelWidth + faceBox.height / modelHeight) / (2.0f * faceboxScaleFactor);
		// translate the model:
		Scalar meanX = cv::mean(xCoords);
		double meanXd = meanX[0];
		Scalar meanY = cv::mean(yCoords);
		double meanYd = meanY[0];
		// move it:
		xCoords += faceBox.x + faceBox.width / 2.0f - meanXd;
		yCoords += faceBox.y + faceBox.height / 1.8f - meanYd; // we use another value for y because we don't want to center the model right in the middle of the face-box
		*/
		return modelShape;
	};

	// out: aligned modelShape
	// in: Rect, ocv with tl x, tl y, w, h (?) and calcs center
	// directly modifies modelShape
	// could move to parent-class
	// assumes mean -0.5, 0.5 and just places inside FB
	// TODO: Actually this function uses model.mean as well as a modelShape input, this is
	// a big ambiguous. Move this function out of this class? But we need access to getLandmarkAsPoint?
	// Also think about the alignRigid function above.
	// @throws ...exception When we can't align (e.g. the given landmarks are 2 eyes that are on top of each other, so sx and sy are not calculable).
	cv::Mat alignRigid(cv::Mat modelShape, imageio::LandmarkCollection landmarks) const {
		// we assume we get passed a col-vec. For convenience, we keep it.
		if (modelShape.cols != 1) {
			throw std::runtime_error("The supplied model shape does not have one column (i.e. it doesn't seem to be a column-vector).");
			// We could also check if it's a row-vector and if yes, transpose.
		}
		Mat xCoords = modelShape.rowRange(0, modelShape.rows / 2);
		Mat yCoords = modelShape.rowRange(modelShape.rows / 2, modelShape.rows);
		
		Mat modelLandmarksX, modelLandmarksY, alignmentLandmarksX, alignmentLandmarksY;
		for (auto&& lm : landmarks.getLandmarks()) {
			cv::Point2f p;
			// What follows is some ugly hackery because the eye-center points from PaSC are not defined in the
			// model. Find a more generic solution for this! As it is really a special case, maybe add a flag parameter to this function?
			// We create a landmark-point in the model at the eye centers that doesn't exist, and use this one to align the model
			if (lm->getName() == "le") {
				cv::Point2f reye_oc = model.getLandmarkAsPoint("37"); // Note: The points might not exist in the model
				cv::Point2f reye_ic = model.getLandmarkAsPoint("40");
				cv::Point2f reye_center = cv::Vec2f(reye_oc + reye_ic) / 2.0f;
				p = reye_center;
			}
			else if (lm->getName() == "re") {
				cv::Point2f leye_oc = model.getLandmarkAsPoint("46");
				cv::Point2f leye_ic = model.getLandmarkAsPoint("43");
				cv::Point2f leye_center = cv::Vec2f(leye_oc + leye_ic) / 2.0f;
				p = leye_center;
			}
			else {
				p = model.getLandmarkAsPoint(lm->getName());
			}
			modelLandmarksX.push_back(p.x);
			modelLandmarksY.push_back(p.y);
			alignmentLandmarksX.push_back(lm->getX());
			alignmentLandmarksY.push_back(lm->getY());
		}

		// Note: Calculate the scaling first, then scale, then calculate the translation.
		// Because the translation will change once we scale the model (the centroid of our
		// two points is not at the centroid of the whole model (which is the point from where we scale). (ZF)
		float sx = calculateScaleRatio(modelLandmarksX, alignmentLandmarksX);
		float sy = calculateScaleRatio(modelLandmarksY, alignmentLandmarksY);
		float s;
		// Note: If the y-difference is very small (instead of zero), the sx or sy number could be
		// very large. This could cause side-effects?
		// 'isnormal': Determines if the given floating point number arg is normal, i.e. is neither zero, subnormal, infinite, nor NaN.
		if (!std::isnormal(sx) && !std::isnormal(sy)) {
			// sx and sy are both not calculable, i.e. we can't align (e.g. the given landmarks are 2 eyes that are on top of each other).
			// This happens in very rare cases (1 image so far on PaSC)
			throw std::runtime_error("x- and y-scale both not calculable, cannot align the model."); // we should use a derived exception here
		}
		// Now at least one of sx and sy is normal:
		if (!std::isnormal(sx)) { 
			s = sy;
		}
		else if (!std::isnormal(sy)) {
			s = sx;
		}
		else {
			s = (sx + sy) / 2.0f;
		}

		modelLandmarksX *= s;
		modelLandmarksY *= s;
		float tx = calculateMeanTranslation(modelLandmarksX, alignmentLandmarksX);
		float ty = calculateMeanTranslation(modelLandmarksY, alignmentLandmarksY);

		xCoords = (xCoords * s + tx);
		yCoords = (yCoords * s + ty);

		return modelShape;
	};

	// out: optimized model-shape
	// in: GRAY img
	// in: evtl zusaetzlicher param um scale-level/iter auszuwaehlen
	// calculates shape updates (deltaShape) for one or more iter/scales and returns...
	// assume we get a col-vec.
	cv::Mat optimize(cv::Mat modelShape, cv::Mat image) {

		for (int cascadeStep = 0; cascadeStep < model.getNumCascadeSteps(); ++cascadeStep) {
			//feature_current = obtain_features(double(TestImg), New_Shape, 'HOG', hogScale);

			vector<cv::Point2f> points;
			for (int i = 0; i < model.getNumLandmarks(); ++i) { // in case of HOG, need integers?
				points.emplace_back(cv::Point2f(modelShape.at<float>(i), modelShape.at<float>(i + model.getNumLandmarks())));
			}
			Mat currentFeatures;
			double dynamicFaceSizeDistance = 0.0;
			if (true) { // adaptive
				// dynamic face-size:
				cv::Vec2f point1(modelShape.at<float>(8), modelShape.at<float>(8 + model.getNumLandmarks())); // reye_ic
				cv::Vec2f point2(modelShape.at<float>(9), modelShape.at<float>(9 + model.getNumLandmarks())); // leye_ic
				cv::Vec2f anchor1 = (point1 + point2) / 2.0f;
				cv::Vec2f point3(modelShape.at<float>(11), modelShape.at<float>(11 + model.getNumLandmarks())); // rmouth_oc
				cv::Vec2f point4(modelShape.at<float>(12), modelShape.at<float>(12 + model.getNumLandmarks())); // lmouth_oc
				cv::Vec2f anchor2 = (point3 + point4) / 2.0f;
				// dynamic window-size:
				// From the paper: patch size $ S_p(d) $ of the d-th regressor is $ S_p(d) = S_f / ( K * (1 + e^(d-D)) ) $
				// D = numCascades (e.g. D=5, d goes from 1 to 5 (Matlab convention))
				// K = fixed value for shrinking
				// S_f = the size of the face estimated from the previous updated shape s^(d-1).
				// For S_f, can use the IED, EMD, or max(IED, EMD). We use the EMD.
				dynamicFaceSizeDistance = cv::norm(anchor1 - anchor2);
				double windowSize = dynamicFaceSizeDistance / 2.0; // shrink value
				double windowSizeHalf = windowSize / 2.0;
				windowSizeHalf = std::round(windowSizeHalf * (1 / (1 + exp((cascadeStep + 1) - model.getNumCascadeSteps())))); // this is (step - numStages), numStages is 5 and step goes from 1 to 5. Because our step goes from 0 to 4, we add 1.
				int NUM_CELL = 3; // think about if this should go in the descriptorExtractor or not. Is it Hog specific?
				int windowSizeHalfi = static_cast<int>(windowSizeHalf) + NUM_CELL - (static_cast<int>(windowSizeHalf) % NUM_CELL); // make sure it's divisible by 3. However, this is not needed and not a good way
				
				currentFeatures = model.getDescriptorExtractor(cascadeStep)->getDescriptors(image, points, windowSizeHalfi);
			}
			else { // non-adaptive, the descriptorExtractor has all necessary params
				currentFeatures = model.getDescriptorExtractor(cascadeStep)->getDescriptors(image, points);
			}
			currentFeatures = currentFeatures.reshape(0, currentFeatures.cols * model.getNumLandmarks()).t();

			//delta_shape = AAM.RF(1).Regressor(hogScale).A(1:end - 1, : )' * feature_current + AAM.RF(1).Regressor(hogScale).A(end,:)';
			Mat regressorData = model.getRegressorData(cascadeStep);
			//Mat deltaShape = regressorData.rowRange(0, regressorData.rows - 1).t() * currentFeatures + regressorData.row(regressorData.rows - 1).t();
			Mat deltaShape = currentFeatures * regressorData.rowRange(0, regressorData.rows - 1) + regressorData.row(regressorData.rows - 1);
			if (true) { // adaptive
				modelShape = modelShape + deltaShape.t() * dynamicFaceSizeDistance;
			}
			else {
				modelShape = modelShape + deltaShape.t();
			}
			
			/*
			for (int i = 0; i < m.getNumLandmarks(); ++i) {
			cv::circle(landmarksImage, Point2f(modelShape.at<float>(i, 0), modelShape.at<float>(i + m.getNumLandmarks(), 0)), 6 - hogScale, Scalar(51.0f*(float)hogScale, 51.0f*(float)hogScale, 0.0f));
			}*/
		}

		return modelShape;
	};

private:
	SdmLandmarkModel model;
};


} /* namespace shapemodels */
#endif /* SDMLANDMARKMODEL_HPP_ */
