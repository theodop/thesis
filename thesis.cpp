#include <opencv2/opencv.hpp> // Open Computer Vision platform
#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include <alsa/asoundlib.h>     // for sound
#include <ncurses.h>            // for input + output
#include <thread>               // used for sound thread
#include <math.h>               // used for trig functions
#include <ctime>                // for performance timing
#include <librealsense2/rsutil.h>

#include "cv-helpers.cpp"
#include "visualisation.cpp"
#include "audio.cpp"
#include "sampling.cpp"

// input codes for our Logitech clicker
#define CLICKER_LEFT 339
#define CLICKER_RIGHT 338
#define CLICKER_POWER 269

void setup_input()
{
  initscr(); /* Start curses mode 		  */

  keypad(stdscr, TRUE);
  mousemask(ALL_MOUSE_EVENTS, NULL);
}

void cleanup()
{
  endwin();
  if (pcm_handle != NULL)
  {
    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
  }
}


void play_startup_sound(){
  audio_pointers_count = 1;
  audio_pointers[0].delayms = 0;
  audio_pointers[0].left_amount = 0.7;
  audio_pointers[0].right_amount = 0.7;
  audio_pointers[0].sound_index = 1;
  sound_ready = true;
}

void loop()
{
  while (1)
  {
    int ch = getch();
    if (ch != -1)
    {
      switch (ch)
      {
      case CLICKER_LEFT:
        low_power_mode = false;
        ready_for_sample = true;
        break;
      case CLICKER_RIGHT:
        low_power_mode = false;
        ready_for_sample = true;
        break;
      // for some reason, this requires 2 clicks of the button.
      // I'm calling this a feature not a bug
      case CLICKER_POWER:
        low_power_mode = true;
        break;
      }
    }
  }
}

int main(int argc, char *argv[]) try
{
  
  setup_input();
  printw("Input configured \n");
  printw("Reading audio file from clap.wav\n");
  read_audio_file("clap.wav", SOUND_INDEX_CLAP);
  printw("Reading audio file from ready.wav\n");
  read_audio_file("ready.wav",SOUND_INDEX_STARTUP);
  printw("Reading beeps audio files\n");
  read_audio_file("1beep.wav",SOUND_INDEX_1BEEP);
  read_audio_file("2beep.wav",SOUND_INDEX_2BEEP);
  read_audio_file("3beep.wav",SOUND_INDEX_3BEEP);
  printw("Configuring audio...\n");
  refresh();

  if (argc <= 1)
  {
    setup_audio(PCM_DEFAULT_DEVICE);
  }
  else
  {
    setup_audio(argv[1]);
  }
  refresh();

  printw("Starting depth camera...\n");
  refresh();

  using namespace cv;
  // Create a Pipeline - this serves as a top-level API for streaming and processing frames
  p = new rs2::pipeline();
  // Configure and start the pipeline
  rs2::pipeline_profile selection = p->start();

  printw("%i profiles found\n",selection.get_streams().size());
  refresh();

  for (auto s : selection.get_streams()) {
    printw(s.stream_name().c_str()); printw("\n"); refresh();
  }

  auto depth_stream = selection.get_stream(RS2_STREAM_DEPTH)
                          .as<rs2::video_stream_profile>();

  auto intrins = depth_stream.get_intrinsics();
  float fov[2]; // X, Y fov
  rs2_fov(&intrins, fov);
  fovwidth = fov[0];
  fovheight = fov[1];
  printw("Depth camera initialised with FOV %f (horiz) %f (vert)", fov[0], fov[1]);

  play_startup_sound();

  visualisation_init();

  start_sampling_thread();

  loop();

  refresh();
  sleep(5);
  cleanup();
  return EXIT_SUCCESS;
}
catch (const rs2::error &e)
{
  printw("RealSense error: ");
  printw(e.what());
  refresh();
  usleep(5000000);
  cleanup();
  //std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
  return EXIT_FAILURE;
}
catch (const std::exception &e)
{
  printw("Unhandled exception: ");
  printw(e.what());
  refresh();
  usleep(5000000);
  cleanup();
  // std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
