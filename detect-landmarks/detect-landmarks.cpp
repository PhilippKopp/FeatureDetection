/*
 * detect-landmarks.cpp
 *
 *  Created on: 20.03.2014
 *      Author: Patrik Huber
 *
 *  Example command-line arguments to run:
 *    detect-landmarks -v -i C:\\Users\\Patrik\\Documents\\Github\\data\\iBug_lfpw\\testset\\ -m "C:\Users\Patrik\Documents\GitHub\sdm_lfpw_tr100_20lm_10s_5c_vlhogUoctti_3_12_4_NEW.txt" -f "C:\opencv\opencv_2.4.8\sources\data\haarcascades\haarcascade_frontalface_alt2.xml" -o C:\\Users\\Patrik\\Documents\\GitHub\\sdmLmsOut\\
 */

#include <chrono>
#include <memory>
#include <iostream>
#include <iomanip>
#include <stdexcept>

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/objdetect/objdetect.hpp"

#ifdef WIN32
	#define BOOST_ALL_DYN_LINK	// Link against the dynamic boost lib. Seems to be necessary because we use /MD, i.e. link to the dynamic CRT.
	#define BOOST_ALL_NO_LIB	// Don't use the automatic library linking by boost with VS2010 (#pragma ...). Instead, we specify everything in cmake.
#endif
#include "boost/program_options.hpp"
#include "boost/property_tree/ptree.hpp"
#include "boost/property_tree/info_parser.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/lexical_cast.hpp"

#include "superviseddescent/SdmLandmarkModel.hpp"
#include "superviseddescent/LandmarkBasedSupervisedDescentTraining.hpp" // Todo: move the free functions there somewhere else and then remove this include

#include "imageio/ImageSource.hpp"
#include "imageio/FileImageSource.hpp"
#include "imageio/FileListImageSource.hpp"
#include "imageio/DirectoryImageSource.hpp"
#include "imageio/CameraImageSource.hpp"
#include "imageio/SimpleModelLandmarkSink.hpp"
#include "imageio/LandmarkSource.hpp"
#include "imageio/DefaultNamedLandmarkSource.hpp"
#include "imageio/SimpleRectLandmarkFormatParser.hpp"
#include "imageio/PascStillEyesLandmarkFormatParser.hpp"
#include "imageio/PascVideoEyesLandmarkFormatParser.hpp"
#include "imageio/SimpleModelLandmarkFormatParser.hpp"
#include "imageio/LandmarkFileGatherer.hpp"
#include "imageio/ModelLandmark.hpp"

#include "logging/LoggerFactory.hpp"

using namespace imageio;
using namespace superviseddescent;
namespace po = boost::program_options;
using std::cout;
using std::endl;
using std::shared_ptr;
using std::make_shared;
using boost::property_tree::ptree;
using boost::filesystem::path;
using boost::lexical_cast;
using cv::Mat;
using cv::Point2f;
using logging::Logger;
using logging::LoggerFactory;
using logging::LogLevel;


template<class T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v)
{
	std::copy(v.begin(), v.end(), std::ostream_iterator<T>(cout, " "));
	return os;
}

int main(int argc, char *argv[])
{
	
	string verboseLevelConsole;
	bool useFileList = false;
	bool useImgs = false;
	bool useDirectory = false;
	bool useFaceDetector = false;
	vector<path> inputPaths;
	path inputFilelist;
	path inputDirectory;
	vector<path> inputFilenames;
	shared_ptr<ImageSource> imageSource;
	path sdmModelFile;
	path faceDetectorFilename;
	path faceBoxesDirectory;
	path outputDirectory;
	string landmarkType;

	try {
		po::options_description desc("Allowed options");
		desc.add_options()
			("help,h",
				"produce help message")
			("verbose,v", po::value<string>(&verboseLevelConsole)->implicit_value("DEBUG")->default_value("INFO","show messages with INFO loglevel or below."),
				  "specify the verbosity of the console output: PANIC, ERROR, WARN, INFO, DEBUG or TRACE")
			("input,i", po::value<vector<path>>(&inputPaths)->required(),
				"input from one or more files, a directory, or a .lst/.txt-file containing a list of images")
			("model,m", po::value<path>(&sdmModelFile)->required(),
				"an SDM model file to load")
			("face-detector,f", po::value<path>(&faceDetectorFilename),
				"path to an XML CascadeClassifier from OpenCV. Either -f or -g is required")
			("face-initialization,g", po::value<path>(&faceBoxesDirectory),
				"path to face-boxes or landmarks to initialize the model. Either -f or -g is required.")
			("landmark-type,t", po::value<string>(&landmarkType),
				"specify the type of landmarks to load: rect-face-box, PaSC-still-PittPatt-eyes, PaSC-video-PittPatt-detections, SimpleModelLandmark")
			("output,o", po::value<path>(&outputDirectory)->required(),
				"Output directory for the result images and landmarks.")
		;

		po::positional_options_description p;
		p.add("input", -1);

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
		if (vm.count("help")) {
			cout << "Usage: detect-landmarks [options]" << endl;
			cout << desc;
			return EXIT_SUCCESS;
		}
		po::notify(vm);
		if (vm.count("face-detector") + vm.count("face-initialization") != 1) {
			cout << "Error while parsing command-line arguments: specify either a face-detector (-f) or face-initialization (-g) as input" << endl;
			cout << desc;
			return EXIT_SUCCESS;
		}

	}
	catch (po::error& e) {
		cout << "Error while parsing command-line arguments: " << e.what() << endl;
		cout << "Use --help to display a list of options." << endl;
		return EXIT_SUCCESS;
	}

	LogLevel logLevel;
	if(boost::iequals(verboseLevelConsole, "PANIC")) logLevel = LogLevel::Panic;
	else if(boost::iequals(verboseLevelConsole, "ERROR")) logLevel = LogLevel::Error;
	else if(boost::iequals(verboseLevelConsole, "WARN")) logLevel = LogLevel::Warn;
	else if(boost::iequals(verboseLevelConsole, "INFO")) logLevel = LogLevel::Info;
	else if(boost::iequals(verboseLevelConsole, "DEBUG")) logLevel = LogLevel::Debug;
	else if(boost::iequals(verboseLevelConsole, "TRACE")) logLevel = LogLevel::Trace;
	else {
		cout << "Error: Invalid LogLevel." << endl;
		return EXIT_FAILURE;
	}
	
	Loggers->getLogger("superviseddescent").addAppender(make_shared<logging::ConsoleAppender>(logLevel));
	Loggers->getLogger("detect-landmarks").addAppender(make_shared<logging::ConsoleAppender>(logLevel));
	Logger appLogger = Loggers->getLogger("detect-landmarks");

	appLogger.debug("Verbose level for console output: " + logging::logLevelToString(logLevel));

	if (faceBoxesDirectory.empty()) {
		useFaceDetector = true;
	} // else, useFaceDetector stays at false

	if (inputPaths.size() > 1) {
		// We assume the user has given several, valid images
		useImgs = true;
		inputFilenames = inputPaths;
	} else if (inputPaths.size() == 1) {
		// We assume the user has given either an image, directory, or a .lst-file
		if (inputPaths[0].extension().string() == ".lst" || inputPaths[0].extension().string() == ".txt") { // check for .lst or .txt first
			useFileList = true;
			inputFilelist = inputPaths.front();
		} else if (boost::filesystem::is_directory(inputPaths[0])) { // check if it's a directory
			useDirectory = true;
			inputDirectory = inputPaths.front();
		} else { // it must be an image
			useImgs = true;
			inputFilenames = inputPaths;
		}
	} else {
		appLogger.error("Please either specify one or several files, a directory, or a .lst-file containing a list of images to run the program!");
		return EXIT_FAILURE;
	}

	if (useFileList==true) {
		appLogger.info("Using file-list as input: " + inputFilelist.string());
		shared_ptr<ImageSource> fileListImgSrc; // TODO VS2013 change to unique_ptr, rest below also
		try {
			fileListImgSrc = make_shared<FileListImageSource>(inputFilelist.string());
		} catch(const std::runtime_error& e) {
			appLogger.error(e.what());
			return EXIT_FAILURE;
		}
		imageSource = fileListImgSrc;
	}
	if (useImgs==true) {
		appLogger.info("Using input images: ");
		vector<string> inputFilenamesStrings;	// Hack until we use vector<path> (?)
		for (const auto& fn : inputFilenames) {
			appLogger.info(fn.string());
			inputFilenamesStrings.push_back(fn.string());
		}
		shared_ptr<ImageSource> fileImgSrc;
		try {
			fileImgSrc = make_shared<FileImageSource>(inputFilenamesStrings);
		} catch(const std::runtime_error& e) {
			appLogger.error(e.what());
			return EXIT_FAILURE;
		}
		imageSource = fileImgSrc;
	}
	if (useDirectory==true) {
		appLogger.info("Using input images from directory: " + inputDirectory.string());
		try {
			imageSource = make_shared<DirectoryImageSource>(inputDirectory.string());
		} catch(const std::runtime_error& e) {
			appLogger.error(e.what());
			return EXIT_FAILURE;
		}
	}

	if (!boost::filesystem::exists(outputDirectory)) {
		boost::filesystem::create_directory(outputDirectory);
	}

	std::unique_ptr<NamedLandmarkSink> landmarkSink(new SimpleModelLandmarkSink());
	//std::unique_ptr<NamedLandmarkSink> landmarkSink = std::make_unique<SimpleModelLandmarkSink>();

	std::chrono::time_point<std::chrono::system_clock> start, end;
	Mat img;

	SdmLandmarkModel lmModel = SdmLandmarkModel::load(sdmModelFile);
	SdmLandmarkModelFitting modelFitter(lmModel);

	// Load either the face detector or the input face boxes:
	cv::CascadeClassifier faceCascade;
	shared_ptr<NamedLandmarkSource> faceboxSource;
	bool alignToFacebox; ///< true means align to a face box, false means align to landmarks (e.g. eyes)
	if (useFaceDetector) {
		if (!faceCascade.load(faceDetectorFilename.string()))
		{
			appLogger.error("Error loading the face detection model.");
			return EXIT_FAILURE;
		}
		alignToFacebox = true;
	}
	else {
		if (boost::iequals(landmarkType, "rect-face-box")) {
			faceboxSource = make_shared<DefaultNamedLandmarkSource>(LandmarkFileGatherer::gather(imageSource, ".txt", GatherMethod::ONE_FILE_PER_IMAGE_DIFFERENT_DIRS, vector<path>{ faceBoxesDirectory }), make_shared<SimpleRectLandmarkFormatParser>());
			alignToFacebox = true;
		}
		else if (boost::iequals(landmarkType, "PaSC-still-PittPatt-eyes")) {
			faceboxSource = make_shared<DefaultNamedLandmarkSource>(LandmarkFileGatherer::gather(nullptr, ".txt", GatherMethod::SEPARATE_FILES, vector<path>{ faceBoxesDirectory }), make_shared<PascStillEyesLandmarkFormatParser>());
			alignToFacebox = false;
		}
		else if (boost::iequals(landmarkType, "PaSC-video-PittPatt-detections")) { // Todo/Note: Not sure this is working?
			faceboxSource = make_shared<imageio::DefaultNamedLandmarkSource>(imageio::LandmarkFileGatherer::gather(nullptr, ".csv", imageio::GatherMethod::SEPARATE_FILES, vector<path>{ faceBoxesDirectory }), make_shared<imageio::PascVideoEyesLandmarkFormatParser>());
			alignToFacebox = false;
		}
		else if (boost::iequals(landmarkType, "SimpleModelLandmark")) {
			faceboxSource = make_shared<DefaultNamedLandmarkSource>(LandmarkFileGatherer::gather(imageSource, ".txt", GatherMethod::ONE_FILE_PER_IMAGE_SAME_DIR, vector<path>{ }), make_shared<SimpleModelLandmarkFormatParser>());
			alignToFacebox = false;
		}
		else {
			appLogger.error("Invalid landmark type given.");
			return EXIT_FAILURE;
		}
		
	}
	
	while (imageSource->next()) {
		start = std::chrono::system_clock::now();
		appLogger.info("Starting to process " + imageSource->getName().string());
		img = imageSource->getImage();
		Mat landmarksImage = img.clone();
		Mat imgGray;
		cvtColor(img, imgGray, cv::COLOR_BGR2GRAY);
		vector<cv::Rect> faces; // used if aligning to a face box
		LandmarkCollection alignmentLandmarks; // used if aligning to landmarks

		if (useFaceDetector) {
			float score, notFace = 0.5;
			// face detection
			//faceCascade.detectMultiScale(img, faces, 1.2, 2, 0, cv::Size(50, 50));
			faceCascade.detectMultiScale(img, faces);
			if (faces.empty()) {
				// no face found, output the unmodified image. don't create a file for the (non-existing) landmarks.
				imwrite((outputDirectory / imageSource->getName().filename()).string(), landmarksImage);
				continue;
			}
		}
		else {
			if (alignToFacebox) {
				imageio::LandmarkCollection facebox = faceboxSource->get(imageSource->getName());
				if (facebox.isEmpty()) {
					// no face found, output the unmodified image. don't create a file for the (non-existing) landmarks.
					imwrite((outputDirectory / imageSource->getName().filename()).string(), landmarksImage);
					continue;
				}
				faces.push_back(facebox.getLandmark()->getRect());
			}
			else {
				// aligning to landmarks:
				path imageName;
				if (boost::iequals(landmarkType, "PaSC-video-PittPatt-detections")) {	
					string frameNumber = imageSource->getName().stem().extension().string();
					frameNumber.erase(0, 1);
					// Pad with zeros, if user entered an image like frame.3.png instead of frame.003.png
					// If it's already 003, nothing will happen.
					std::stringstream ss;
					ss << std::setfill('0') << std::setw(3) << frameNumber;
					frameNumber = ss.str();
					frameNumber = "-" + frameNumber;
					// Big Todo: Abstract this whole stuff into some PaSC-LM or PaSC-LM-Source class, it shouldn't be in the app here?
					imageName = imageSource->getName().stem().stem() / imageSource->getName().stem().stem();
					imageName += frameNumber;
					imageName.replace_extension(".jpg");
					// "name/name-012.jpg"
				}
				else {
					imageName = imageSource->getName();
				}
				LandmarkCollection tmpLms_pascName = faceboxSource->get(imageName);
				alignmentLandmarks = tmpLms_pascName;
				if (alignmentLandmarks.getLandmarks().size() == 0) {
					appLogger.info("No landmark information found for this image. Skipping it.");
					continue;
				}
				// ugly hack to change the lm-id from 'le'/'re' (PittPatt) to our model-format
				// We do this inside the align-function at the moment
				/*for (auto&& lm : tmpLms_pascName.getLandmarks()) {
					if (lm->getName() == "le") {
						alignmentLandmarks.insert(make_shared<imageio::ModelLandmark>("40", lm->getX(), lm->getY()));
					}
					else if (lm->getName() == "re") {
						alignmentLandmarks.insert(make_shared<imageio::ModelLandmark>("43", lm->getX(), lm->getY()));
					}
				}*/
			}
		}
		
		if (alignToFacebox) {
			// draw the best face candidate (or the face from the face box landmarks)
			cv::rectangle(landmarksImage, faces[0], cv::Scalar(0.0f, 0.0f, 255.0f));
		}
		else {
			// draw landmarks...
			for (auto&& lm : alignmentLandmarks.getLandmarks()) {
				lm->draw(landmarksImage);
			}
		}

		// fit the model
		Mat modelShape = lmModel.getMeanShape();
		if (alignToFacebox) {
			modelShape = modelFitter.alignRigid(modelShape, faces[0]);
		}
		else {
			try {
				modelShape = modelFitter.alignRigid(modelShape, alignmentLandmarks);
			}
			catch (std::runtime_error& e) {
				// can't align, rarely happens
				appLogger.warn(e.what());
				continue;
			}
			
		}
		superviseddescent::drawLandmarks(landmarksImage, modelShape, Scalar(0.0f, 0.0f, 255.0f));
		modelShape = modelFitter.optimize(modelShape, imgGray);

		// draw the final result
		superviseddescent::drawLandmarks(landmarksImage, modelShape, Scalar(0.0f, 255.0f, 0.0f));

		// save the image
		path outputFilename = outputDirectory / imageSource->getName().filename();
		imwrite(outputFilename.string(), landmarksImage);
		// write out the landmarks to a file
		LandmarkCollection landmarks = lmModel.getAsLandmarks(modelShape);
		outputFilename.replace_extension(".txt");
		landmarkSink->add(landmarks, outputFilename.string());

		end = std::chrono::system_clock::now();
		int elapsed_mseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
		appLogger.info("Finished processing. Elapsed time: " + lexical_cast<string>(elapsed_mseconds) + "ms.");
	}

	return 0;
}
