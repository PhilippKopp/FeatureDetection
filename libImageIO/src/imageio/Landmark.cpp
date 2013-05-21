/*
 * Landmark.cpp
 *
 *  Created on: 22.03.2013
 *      Author: Patrik Huber
 */

#include "imageio/Landmark.hpp"
#include "opencv2/core/core.hpp"
#include <utility>

using std::make_pair;

namespace imageio {

Landmark::Landmark(string name, Vec3f position, Size2f size, bool visibility) : name(name), position(position), size(size), visibility(visibility)
{
}

Landmark::~Landmark() {}

bool Landmark::isVisible() const
{
	return visibility;
}

string Landmark::getName() const
{
	return name;
}

Vec3f Landmark::getPosition() const
{
	return position;
}

Size2f Landmark::getSize() const
{
	return size;
}

void Landmark::draw(Mat image) const
{
	//cv::Point2i realFfpCenter(patch->c.x+patch->w_inFullImg*thisLm.displacementFactorW, patch->c.y+patch->h_inFullImg*thisLm.displacementFactorH);
	array<bool, 9> symbol = LandmarkSymbols::get(name);
	cv::Scalar color = LandmarkSymbols::getColor(name);
	unsigned int pos = 0;
	for (int currRow = cvRound(position[1])-1; currRow<=cvRound(position[1])+1; ++currRow) {
		for (int currCol = cvRound(position[0])-1; currCol<=cvRound(position[0])+1; ++currCol) {
			if(symbol[pos]==true) {
				image.at<cv::Vec3b>(currRow,currCol)[0] = (uchar)cvRound(255.0f * color.val[0]);
				image.at<cv::Vec3b>(currRow,currCol)[1] = (uchar)cvRound(255.0f * color.val[1]);
				image.at<cv::Vec3b>(currRow,currCol)[2] = (uchar)cvRound(255.0f * color.val[2]);
			}
			++pos;
		}
	}

}

map<string, array<bool, 9>> LandmarkSymbols::symbolMap;
map<string, cv::Scalar> LandmarkSymbols::colorMap;

array<bool, 9> LandmarkSymbols::get(string landmarkName)
{
	if (symbolMap.empty()) {
		array<bool, 9> reye_c		= {	false, true, false,
										false, true, true,
										false, false, false };
		symbolMap.insert(make_pair("right.eye.pupil.center", reye_c));	// Use an initializer list as soon as msvc supports it...

		array<bool, 9> leye_c	= {	false, true, false,
									true, true, false,
									false, false, false };
		symbolMap.insert(make_pair("left.eye.pupil.center", leye_c));

		array<bool, 9> nose_tip	= {	false, false, false,
									false, true, false,
									true, false, true };
		symbolMap.insert(make_pair("center.nose.tip", nose_tip));

		array<bool, 9> mouth_rc	= {	false, false, true,
									false, true, false,
									false, false, true };
		symbolMap.insert(make_pair("right.lips.corner", mouth_rc));

		array<bool, 9> mouth_lc	= {	true, false, false,
									false, true, false,
									true, false, false };
		symbolMap.insert(make_pair("left.lips.corner", mouth_lc));

		array<bool, 9> reye_oc	= {	false, true, false,
									false, true, true,
									false, true, false };
		symbolMap.insert(make_pair("right.eye.corner_outer", reye_oc));

		array<bool, 9> leye_oc	= {	false, true, false,
									true, true, false,
									false, true, false };
		symbolMap.insert(make_pair("left.eye.corner_outer", leye_oc));

		array<bool, 9> mouth_ulb	= {	false, false, false,
										true, true, true,
										false, true, false };
		symbolMap.insert(make_pair("center.lips.upper.outer", mouth_ulb));

		array<bool, 9> nosetrill_r	= {	true, false, false,
										true, true, true,
										false, false, false };
		symbolMap.insert(make_pair("right.nose.wing.tip", nosetrill_r));

		array<bool, 9> nosetrill_l	= {	false, false, true,
										true, true, true,
										false, false, false };
		symbolMap.insert(make_pair("left.nose.wing.tip", nosetrill_l));

		array<bool, 9> rear_DONTKNOW	= {	false, true, true,
											false, true, false,
											false, true, true };
		symbolMap.insert(make_pair("right.ear.DONTKNOW", rear_DONTKNOW)); // right.ear.(antihelix.tip | lobule.center | lobule.attachement)

		array<bool, 9> lear_DONTKNOW	= {	true, true, false,
											false, true, false,
											true, true, false };
		symbolMap.insert(make_pair("left.ear.DONTKNOW", lear_DONTKNOW));

	}
	const auto symbol = symbolMap.find(landmarkName);
	if (symbol == symbolMap.end()) {
		array<bool, 9> unknownLmSymbol	= {	true, false, true,
											false, true, false,
											true, false, true };
		return unknownLmSymbol;
	}
	return symbol->second;
}

cv::Scalar LandmarkSymbols::getColor(string landmarkName)
{
	if (colorMap.empty()) {
		cv::Scalar reye_c(0.0f, 0.0f, 1.0f);
		colorMap.insert(make_pair("right.eye.pupil.center", reye_c));	// Use an initializer list as soon as msvc supports it...

		cv::Scalar leye_c(1.0f, 0.0f, 0.0f);
		colorMap.insert(make_pair("left.eye.pupil.center", leye_c));

		cv::Scalar nose_tip(0.0f, 1.0f, 0.0f);
		colorMap.insert(make_pair("center.nose.tip", nose_tip));

		cv::Scalar mouth_rc(0.0f, 1.0f, 1.0f);
		colorMap.insert(make_pair("right.lips.corner", mouth_rc));

		cv::Scalar mouth_lc(1.0f, 0.0f, 1.0f);
		colorMap.insert(make_pair("left.lips.corner", mouth_lc));

		cv::Scalar reye_oc(0.0f, 0.0f, 0.48f);
		colorMap.insert(make_pair("right.eye.corner_outer", reye_oc));

		cv::Scalar leye_oc(1.0f, 1.0f, 0.0f);
		colorMap.insert(make_pair("left.eye.corner_outer", leye_oc));

		cv::Scalar mouth_ulb(0.63f, 0.75f, 0.9f);
		colorMap.insert(make_pair("center.lips.upper.outer", mouth_ulb));

		cv::Scalar nosetrill_r(0.27f, 0.27f, 0.67f);
		colorMap.insert(make_pair("right.nose.wing.tip", nosetrill_r));

		cv::Scalar nosetrill_l(0.04f, 0.78f, 0.69f);
		colorMap.insert(make_pair("left.nose.wing.tip", nosetrill_l));

		cv::Scalar rear_DONTKNOW(1.0f, 0.0f, 0.52f);
		colorMap.insert(make_pair("right.ear.DONTKNOW", rear_DONTKNOW)); // right.ear.(antihelix.tip | lobule.center | lobule.attachement)

		cv::Scalar lear_DONTKNOW(0.0f, 0.6f, 0.0f);
		colorMap.insert(make_pair("left.ear.DONTKNOW", lear_DONTKNOW));

	}
	const auto symbol = colorMap.find(landmarkName);
	if (symbol == colorMap.end()) {
		cv::Scalar unknownLmColor(0.35f, 0.35f, 0.35f);
		return unknownLmColor;
	}
	return symbol->second;
}

} /* namespace imageio */
