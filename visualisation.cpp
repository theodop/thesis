#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>

bool use_visualisation = false;
bool visualisation_initialised = false;

const char *open_cv_window_1 = "Step 1";
const char *open_cv_window_2 = "Step 2";
const char *open_cv_window_3 = "Step 3";
const char *open_cv_window_4 = "Step 4";

void update_vis() {
  if (use_visualisation) {
    cv::waitKey(10);
  }
}
/*
void export_to_csv(cv::Mat matrix, const char* filepath) 
{
  cv::Mat copy;
  matrix.convertTo(copy, CV_32F);

  std::ofstream myfile(filepath);

  for (int row=0; row<copy.rows; row++) {
    bool isFirst = true;
    for (int col=0; col<copy.cols; col++) {
      if (!isFirst) {
        myfile << ",";
      }
      isFirst = false;
      float value = copy.at<float>(row, col);
      myfile << value;
    }
    myfile << "\n";
  }

  myfile.close();
} */

void visualise_distance(cv::Mat matrix, int window_number)
{
  if (!use_visualisation) return;

  const char* window_name = "INVALID";

  switch(window_number) {
    case 1:
      window_name = open_cv_window_1;
      break;
    case 2:
      window_name = open_cv_window_2;
      break;
    case 3:
      window_name = open_cv_window_3;
      break;
    case 4:
      window_name = open_cv_window_4;
      break;
  }

  cv::Mat scaled = matrix.clone()*40;
  scaled.convertTo(scaled, CV_8UC1);
  cv::applyColorMap(scaled, scaled, 2);

  cv::imshow(window_name, scaled);
}

void visualisation_init()
{
  if (!visualisation_initialised) {
    if (use_visualisation)
    {
      visualisation_initialised = true;
      cv::namedWindow(open_cv_window_1, cv::WINDOW_AUTOSIZE);
      cv::namedWindow(open_cv_window_2, cv::WINDOW_AUTOSIZE);
      cv::namedWindow(open_cv_window_3, cv::WINDOW_AUTOSIZE);
      cv::namedWindow(open_cv_window_4, cv::WINDOW_AUTOSIZE);
      cv::moveWindow(open_cv_window_1,0,0);
      cv::moveWindow(open_cv_window_2,700,0);
      cv::moveWindow(open_cv_window_3,0,500);
      cv::moveWindow(open_cv_window_4,700,500);
      update_vis();
    }
  }
}