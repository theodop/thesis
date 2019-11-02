#pragma once

#include <thread>

std::thread sampling_thread;

rs2::pipeline *p;

float fovwidth;
float fovheight;

bool ready_for_sample = false;

// whether the subsystems should go into low-power mode.
// this is currently set by the input loop and checked
// by the sampling loop
bool low_power_mode = false;

// tracks whether the realsense pipeline is started or stopped.
// this is shut down when we go into low power mode, and otherwise
// started when input is received.
bool rs_pipeline_active = true;

// the edge detection functionality will threshold at this value
// note that the end result may include values higher than this -- 
// this threshold is only applied before the Laplacian is applied
// as depth variance increases dramatically after this value (and hence
// so do false edges)
#define MAX_DEPTH_THRESHOLD 5

#define MAX_LABELS 200
float label_depths[MAX_LABELS];
uint label_counts[MAX_LABELS];

// keeps track of when the user requested sampling be started
std::chrono::time_point<std::chrono::high_resolution_clock> sampling_start_time;
// keeps track of when the last warning was played - this is for hysteresis purposes
// since we don't want to play warnings over one another
std::chrono::time_point<std::chrono::high_resolution_clock> last_warning_played;

// keeps track of the current obstacle class - 1 for close, 2 for mid
int obstacle_class = 999;

#define motion_detection_ms 10000
#define wait_for_warning_after_sample_ms 2000
#define wait_for_warning_after_warning_ms 500

// we will attempt to decimate down to this width, but decimation is an integer value
// so the actual decimated width may be slightly different.
#define DESIRED_FRAME_WIDTH 200

float get_ms(std::chrono::time_point<std::chrono::high_resolution_clock> timer)
{
  auto duration = std::chrono::high_resolution_clock::now() - timer;
  long mus = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
  float ms = ((float)mus)/1e3;
  return ms;
}

void hole_filling_filter(cv::Mat mat) {
  for(int row = 0; row<mat.rows; row++) {
    for (int col = 0; col<mat.cols; col++) {
      float distance = mat.at<float>(row, col);

      if (distance == 0.0 && col!=0) {
        mat.at<float>(row, col) = mat.at<float>(row, col-1); 
      }
    }
  }
}

uint8_t get_overall_obstacle_class_from_thresholds(cv::Mat near, cv::Mat mid)
{
  assert(near.cols == mid.cols);
  assert(near.rows == mid.rows);

  // assume we only want to sample the middle 1/5th
  int samplex_start = 2*near.cols / 5;
  int samplex_finish = 3*near.cols / 5;
  int sampley_start = 2*near.rows / 5;
  int sampley_finish = 3*near.rows / 5;

  int sample_n = (samplex_finish - samplex_start)*(sampley_finish - sampley_start);

  // proportion of zone found so far
  float near_content = 0;
  float mid_content = 0;

  float content_threshold = 0.3; // the overall proportion of content of the sample zone to satisfy an obstacle is present

  for (int x = samplex_start; x<samplex_finish; x++)
  {
    for (int y = sampley_start; y < sampley_finish; y++)
    {
      float nearval = near.at<float>(y,x);
      float midval = mid.at<float>(y,x);
      
      if (nearval > 0.01) {
        near_content += 1.0/sample_n;
        mid_content += 1.0/sample_n;
      } else if (midval > 0.01) {
        mid_content += 1.0/sample_n;
      }

      if (near_content > content_threshold) {
        return 1;
      }
    }
  }

  return (mid_content > content_threshold) ? 2 : 3;
}

cv::Mat convert_to_opencvmat(rs2::depth_frame depth)
{
  return depth_frame_to_meters(*p, depth);
}

void sample(bool user_triggered)
{
  using namespace cv;
  using Clock=std::chrono::high_resolution_clock;
  if (user_triggered) { 
    clear();
    // set our obstacle class back to 999 to reset
    obstacle_class = 999;
    printw("Time since warning: %f", get_ms(last_warning_played));
    last_warning_played = std::chrono::high_resolution_clock::now();
  }
  auto stopwatch = Clock::now();
  if (user_triggered) printw("[%f]: Waiting for frame\n", get_ms(stopwatch));
  // Block program until frames arrive
  rs2::frameset frames = p->wait_for_frames();

  if (use_visualisation) {
    rs2::video_frame img = frames.get_color_frame();
    imshow(open_cv_window_1, frame_to_mat(img));
  }

  if (user_triggered) printw("[%f]: Captured frame\n", get_ms(stopwatch));

  // Try to get a frame of a depth image
  rs2::depth_frame depth = frames.get_depth_frame();

  // Decimate the frame to reduce the dataset size
  if (depth.get_width() > DESIRED_FRAME_WIDTH) {
    int pre_width = depth.get_width();
    int decimation_amount = pre_width / DESIRED_FRAME_WIDTH;
    rs2::decimation_filter decimation_filter;
    decimation_filter.set_option(RS2_OPTION_FILTER_MAGNITUDE, decimation_amount);
    depth = decimation_filter.process(depth);

    if (user_triggered) printw("[%f]: Decimated frame\n", get_ms(stopwatch));
  }

  // convert to an OpenCV matrix of meters
  auto distances = convert_to_opencvmat(depth);

  if (user_triggered) printw("[%f]: Converted to matrix\n", get_ms(stopwatch));

  cv::medianBlur(distances, distances, 5);

  if (user_triggered) printw("[%f]: Median filter applied\n", get_ms(stopwatch));

  hole_filling_filter(distances);

  if (user_triggered) printw("[%f]: Hole-filling filter applied\n", get_ms(stopwatch));
  visualise_distance(distances, 2);

  auto distance_vect = distances.reshape(1, distances.rows * distances.cols);
  distance_vect.convertTo(distance_vect, CV_32F);

  Mat laplaced;
  distances.convertTo(laplaced, CV_8U);
  visualise_distance(distances, 2);

  for (uint i=0; i<laplaced.rows*laplaced.cols; i++) {
    if (laplaced.at<uint8_t>(i) > MAX_DEPTH_THRESHOLD) {
      laplaced.at<uint8_t>(i) = MAX_DEPTH_THRESHOLD;
    }
  }

  if (user_triggered) printw("[%f]: Clamped maximum depth\n", get_ms(stopwatch));
  // cv::GaussianBlur(laplaced, laplaced, cv::Size(7,7), 0, 0);
  cv::Laplacian(laplaced, laplaced, CV_8U, 3, 1, 0, BORDER_DEFAULT);

  if (user_triggered) printw("[%f]: Laplacian applied\n", get_ms(stopwatch));

  auto edge_strel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3,3));

  // dilate to close any small gaps in the edges, like near the window border
  cv::dilate(laplaced, laplaced, edge_strel);

  for (uint i=0; i<laplaced.rows * laplaced.cols; i++) {
    bool edge = laplaced.at<uint8_t>(i) != 0;

    laplaced.at<uint8_t>(i) = edge ? 0 : 1;
  }

  auto labelled = laplaced.clone();
  // apply unique labels to the sections enclosed in edges
  cv::connectedComponents(laplaced, labelled);

  uint max_label = 0;

  // accumulate the depths for each label
  for (uint i = 0; i <labelled.rows*labelled.cols; i++) {
    uint labeli = labelled.at<uint>(i);
    if (max_label < labeli) max_label = labeli;

    label_depths[labeli] += distances.at<float>(i);
    label_counts[labeli]++;
  }

  if (max_label >= MAX_LABELS) max_label = MAX_LABELS -1;

  uint largest_depth = 0;

  // take the mean
  for (uint labeli = 0; labeli<=max_label; labeli++) {
    label_depths[labeli] = label_depths[labeli] / label_counts[labeli];
  }

  if (user_triggered) printw("[%f]: Assigned %u labels\n", get_ms(stopwatch), max_label);
  

  // in one step, fill any pixels marked as edges from the left or top
  // (since these weren't labelled properly in the labelling step) and
  // also apply the label means to the distances array
  for (uint row=0; row<labelled.rows; row++) {
    for (uint col=0; col<labelled.cols; col++) {
      // flood fill every 2nd value from the left
      if (laplaced.at<uint8_t>(row,col)==0) {
        if (col!=0) {
          laplaced.at<uint8_t>(row,col) = laplaced.at<uint8_t>(row, col-1);
        }
      } else {
        uint labeli = labelled.at<uint>(row,col);
        float mean_depth = label_depths[labeli];
        distances.at<float>(row, col) = mean_depth;
      }
    }
  }

  visualise_distance(labelled, 3);
  
  // perform object detection through distance classification

  float nearThresh = 1;
  float midThresh = 2.5;

  cv::Mat near = distances.clone();
  near = near.reshape(1, near.rows);
  cv::Mat mid = near.clone();

  for (uint i=0; i<distances.rows * distances.cols; i++) {
    if (near.at<float>(i) < nearThresh) {
      near.at<float>(i) = nearThresh;
      mid.at<float>(i) = midThresh;
    } else if (near.at<float>(i) < midThresh) {
      near.at<float>(i) = 0;
      mid.at<float>(i) = midThresh;
    } else {
      near.at<float>(i) = 0;
      mid.at<float>(i) = 0;
    }
  }

  int new_obstacle_class = get_overall_obstacle_class_from_thresholds(near, mid);

  if (user_triggered) printw("[%f]: obstacle class: %d\n", get_ms(stopwatch), obstacle_class);

  visualise_distance(distances, 4);

  if (user_triggered) printw("[%f]: thresholded\n", get_ms(stopwatch));

  if (user_triggered) printw("[%f]: classified\n", get_ms(stopwatch));

  if (!user_triggered && 
    (get_ms(sampling_start_time) > wait_for_warning_after_sample_ms) && 
    (get_ms(last_warning_played) > wait_for_warning_after_warning_ms) || new_obstacle_class < obstacle_class) {
    obstacle_class = new_obstacle_class;
    switch (obstacle_class) {
      case 1:
        audio_pointers_count = 1;
        audio_pointers[0].delayms = 0;
        audio_pointers[0].sound_index = SOUND_INDEX_2BEEP;
        audio_pointers[0].left_amount = 0.5;
        audio_pointers[0].right_amount = 0.5;
        last_warning_played = Clock::now();
        sound_ready = true;
        break;
      case 2:
        audio_pointers_count = 1;
        audio_pointers[0].delayms = 0;
        audio_pointers[0].sound_index = SOUND_INDEX_1BEEP;
        audio_pointers[0].left_amount = 0.5;
        audio_pointers[0].right_amount = 0.5;
        last_warning_played = Clock::now();
        sound_ready = true;
        break;
    }
  }

  // create our "clap" audio pointers
  if (user_triggered) {
    // Get the depth frame's dimensions
    float width = distances.cols;
    float height = distances.rows;
    float x_pixels_per_theta = width / fovwidth;

    // fill our samples
    audio_pointers_count = 3;
    int sample_thetas[audio_pointers_count] = {-32, 0, 32};

    printw("Captured frame with width %f\n", width);

    for (int i = 0; i < audio_pointers_count; i++)
    {
      float theta = sample_thetas[i];

      // convert our sample theta to a pixel value
      // e.g. given an FOV of 90, and a theta of -30, we are about 16.7% across
      // the depth frame
      int x = width * (theta + fovwidth / 2) / fovwidth;
      int y = height / 2;

      audio_pointers[i].delayms = distances.at<float>(y, x) * METERS_TO_DELAY_MS;
      audio_pointers[i].sound_index = 0; // set this pointer to be our clap sound

      // convert theta to rads by multiplying by pi/180
      float theta_rads = theta * 0.01745329f;

      // using constant power panning, the sound is panned to the left and right
      // ears using trigonometric rules and scaled by sqrt(2)/2.
      audio_pointers[i].left_amount = 0.707107f * (cos(theta_rads) - sin(theta_rads));
      audio_pointers[i].right_amount = 0.707107f * (cos(theta_rads) + sin(theta_rads));

      if (audio_pointers[i].delayms == 0)
      {
        audio_pointers[i].left_amount = 0;
        audio_pointers[i].right_amount = 0;
      } 
      // if this pointer has exactly the same delay as the one to the left, add a little delay
      else if (i == 2 && audio_pointers[i].delayms == audio_pointers[0].delayms)
      {
        audio_pointers[i].delayms += 50;
      }
      else if (i > 0 && audio_pointers[i].delayms == audio_pointers[i-1].delayms)
      {
        audio_pointers[i].delayms += 50;
      }
      
      printw("Created sample at x=%d, y=%d, theta=%f, %dms delay, volume %f %f\n",
        x,
        y,
        theta,
        audio_pointers[i].delayms,
        audio_pointers[i].left_amount,
        audio_pointers[i].right_amount
        );

      refresh();
    }

    sound_ready = true;

    if (user_triggered) {
      sampling_start_time = Clock::now();
    }
  }

  update_vis();
}

void reset_labels(){
  // initialise the mean depths to 0
  for (uint labeli = 0; labeli<MAX_LABELS; labeli++) {
    label_depths[labeli] = 0;
    label_counts[labeli] = 0;
  }
}

void sampling_loop()
{
    bool in_detection_mode = false;
    last_warning_played = std::chrono::high_resolution_clock::now();
    while (1) {
      // if we're in low power mode, check to see if the pipeline should
      // be shut down
      if (low_power_mode) {
        if (rs_pipeline_active) {
          clear();
          printw("Putting pipeline into low-power mode.\n");
          refresh();
          p->stop();
          rs_pipeline_active = false;
          // play a shutdown sound
          audio_pointers_count = 1;
          audio_pointers[0].sound_index = SOUND_INDEX_2BEEP;
          audio_pointers[0].left_amount = 0.5;
          audio_pointers[0].right_amount = 0.5;
          audio_pointers[0].delayms = 0;
          sound_ready = true;
        }

        usleep(100000);
      } else {
        // if we've been asked to capture but the pipeline is in low power mode,
        // wake it back up
        if (!rs_pipeline_active) {
          p->start();
          rs_pipeline_active = true;
          audio_pointers_count = 1;
          audio_pointers[0].sound_index = SOUND_INDEX_3BEEP;
          audio_pointers[0].left_amount = 0.5;
          audio_pointers[0].right_amount = 0.5;
          audio_pointers[0].delayms = 100;
          sound_ready = true;
          // we can't capture a sample if the pipeline has just started
          ready_for_sample = false;
          in_detection_mode = false;
        }
        else if (ready_for_sample) {
          in_detection_mode = true;
          sample(true);
          reset_labels();
          ready_for_sample = false;
        } else if (in_detection_mode && (get_ms(sampling_start_time) < motion_detection_ms)) {
          sample(false); 
          reset_labels();
        } else if (in_detection_mode) {
          // once we've finished detection, play a sound indicating motion detection has finished
          in_detection_mode = false;

          audio_pointers_count = 1;
          audio_pointers[0].sound_index = SOUND_INDEX_3BEEP;
          audio_pointers[0].left_amount = 0.5;
          audio_pointers[0].right_amount = 0.5;
          audio_pointers[0].delayms = 500;
          sound_ready = true;
        }
        usleep(10000);
      }
    }
}

void start_sampling_thread()
{
  sampling_thread = std::thread(&sampling_loop);
}