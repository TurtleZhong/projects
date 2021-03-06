﻿#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

void find_feature_matches(Mat& img1, Mat& img2,
                          vector<KeyPoint>& keypoints1,
                          vector<KeyPoint>& keypoints2,
                          vector<DMatch>& matches,
                          string method = "KNNsearch");
void pose_estimation_2d2d(vector<KeyPoint> keypoints1, vector<KeyPoint> keypoints2,
                          vector<DMatch> matches, Mat& R, Mat& t);


void triangulation(const vector<KeyPoint>& keypoints1,
                   const vector<KeyPoint>& keypoints2,
                   const vector<DMatch>& matches,
                   const Mat& R, const Mat& t,
                         vector<Point3d>& points);
Point2f pixel2cam ( const Point2d& p, const Mat& K );

int main(int argc, char *argv[])
{
    cout << "OpenCV version: "
                << CV_MAJOR_VERSION << "."
                << CV_MINOR_VERSION << "."
                << CV_SUBMINOR_VERSION
                << std::endl;

    Mat img1 = imread("../1.png", 1);
    Mat img2 = imread("../2.png" , 1);
    vector<KeyPoint> keypoints1, keypoints2;
    vector<DMatch> matches;

    find_feature_matches(img1, img2, keypoints1, keypoints2, matches, "KNN");
    Mat R,t;
    pose_estimation_2d2d(keypoints1, keypoints2, matches, R, t);




    waitKey(0);
    return 0;
}

/*
 * input: image1,image2
 * output: keypoints1,keypoints2,goodmatches
 * function: detect the feature use ORB meathod
 */
void find_feature_matches(Mat& img1, Mat& img2,
                          vector<KeyPoint>& keypoints1,
                          vector<KeyPoint>& keypoints2,
                          vector<DMatch>& matches,
                          string method)
{
    /*
      * define the detect param: use ORB
      */
    Ptr<ORB> orb = ORB::create();
    Mat descriptors;
    /*
      * use detect() function to detect keypoints
      */
    orb-> detect(img1, keypoints1);
    /*
      * conpute the extractor and show the keypoints
      */
    orb-> compute(img1, keypoints1, descriptors);

    Mat testDescriptors;
    orb->detect(img2, keypoints2);
    orb->compute(img2, keypoints2,testDescriptors);

    /*
      * FLANN
      */
    if(method == "KNN" || method == "knn")
    {
        flann::Index flannIndex(testDescriptors, flann::LshIndexParams(12, 20, 2), cvflann::FLANN_DIST_HAMMING);



        /*Match the feature*/
        Mat matchIndex(descriptors.rows, 2, CV_32SC1);
        Mat matchDistance(descriptors.rows, 2, CV_32SC1);
        flannIndex.knnSearch(descriptors, matchIndex, matchDistance, 2, flann::SearchParams());

        //vector<DMatch> goodMatches;
        for (int i = 0; i < matchDistance.rows; i++)
        {
            if(matchDistance.at<float>(i,0) < 0.6 * matchDistance.at<float>(i, 1))
            {
                DMatch dmatchs(i, matchIndex.at<int>(i,0), matchDistance.at<float>(i,1));
                matches.push_back(dmatchs);
            }
        }

    }
    else if(method == "BF")
    {
        vector<DMatch> tmpMatches;
        BFMatcher matcher(NORM_HAMMING);
        matcher.match(descriptors, testDescriptors, tmpMatches);
        double min_dist = 10000, max_dist = 0;
        for(int i = 0; i < descriptors.rows; i++)
        {
            double dist = tmpMatches[i].distance;
            if(dist < min_dist) min_dist = dist;
            if(dist > max_dist) max_dist = dist;
        }
        cout << "--Max dist = " << max_dist << endl;
        cout << "--Min dist = " << min_dist << endl;

        for(int i =0; i < descriptors.rows; i++)
        {
            if(tmpMatches[i].distance <= max(2*min_dist, 30.0))
            {
                matches.push_back(tmpMatches[i]);
            }
        }

    }

    Mat resultImage;
    drawMatches(img1, keypoints1, img2, keypoints2, matches, resultImage);
    imshow("result of Image", resultImage);

    cout << "We got " << matches.size() << " good Matchs" << endl;
}

void pose_estimation_2d2d(vector<KeyPoint> keypoints1, vector<KeyPoint> keypoints2,
                          vector<DMatch> matches, Mat& R, Mat& t)
{
    /*
     * camera internal param
     */
    Mat K = (Mat_<double> (3,3) << 520.9, 0, 325.1, 521.0, 249.7, 0, 0, 1);
    vector<Point2f> points1, points2;

    for(int i = 0; i < matches.size(); i++)
    {
        points1.push_back(keypoints1[matches[i].queryIdx].pt);
        points2.push_back(keypoints2[matches[i].trainIdx].pt);
    }

    /*
     * compute the F (fundmental) Matrix
     */
    Mat fundamentalMatrix;
    fundamentalMatrix = findFundamentalMat(points1, points2, CV_FM_8POINT);
    cout << "fundamental matrix is " << endl << fundamentalMatrix << endl;

    /*
     * compute the E (essential) Matrix opencv2.4.11 could not find this function
     * update: we use the opencv 3.1.0 version
     */
    Point2d principalPoint(325.1, 249.7);
    int focalLength = 521;
    Mat essentialMatrix = findEssentialMat(points1, points2, focalLength, principalPoint, RANSAC);
    cout << "essential matrix is " << endl << essentialMatrix << endl;

    /*
     * compute the homography Matrix
     */
    Mat homographyMatrix;
    homographyMatrix = findHomography(points1, points2);
    cout << "homography Matrix is " << endl << homographyMatrix << endl;

    /*
     * recover the R and t from the essential matrxi
     */
    recoverPose(essentialMatrix, points1, points2, R, t, focalLength, principalPoint);
    cout << "R is " << endl << R << endl;
    cout << "t is " << endl << t << endl;


}

void triangulation(const vector<KeyPoint>& keypoints1,
                   const vector<KeyPoint>& keypoints2,
                   const vector<DMatch>& matches,
                   const Mat& R, const Mat& t,
                         vector<Point3d>& points)
{
    Mat T1 = (Mat_<double>(3,4) << 1,0,0,0,
                                   0,1,0,0,
                                   0,0,1,0);
    Mat T2 = (Mat_<double>(3,4) <<
              R.at<double>(0,0), R.at<double>(0,1), R.at<double>(0,2), t.at<double>(0,0),
              R.at<double>(1,0), R.at<double>(1,1), R.at<double>(1,2), t.at<double>(1,0),
              R.at<double>(2,0), R.at<double>(2,1), R.at<double>(2,2), t.at<double>(2,0),);

    Mat K = (Mat_<double> (3,3) << 520.9, 0, 325.1, 521.0, 249.7, 0, 0, 1);
    vector<Point2d> pts1,pts2;

    for(DMatch m: matches)
    {
        pts1.push_back( pixel2cam(keypoints1[m.queryIdx].pt, K));
        pts2.push_back( pixel2cam(keypoints2[m.trainIdx].pt, K));
    }

    Mat pts4d;
    triangulatePoints(T1, T2, pts1, pts2, pts4d);

    for(int i = 0; i < pts4d.cols; i++)
    {
        Mat x = pts4d.col(i);
        x /= x.at<float>(3,0);
        Point3d p (x.at<float>(0,0), x.at<float>(1,0), x.at<float>(2,0));
        points.push_back(p);
    }

}


Point2f pixel2cam ( const Point2d& p, const Mat& K )
{
    return Point2f
    (
        ( p.x - K.at<double>(0,2) ) / K.at<double>(0,0),
        ( p.y - K.at<double>(1,2) ) / K.at<double>(1,1)
    );
}

















