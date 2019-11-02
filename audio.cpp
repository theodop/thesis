// this is the PCM device as listed by the ALSA subsystem in Linux
// can be overridden using a command line parameter
#define PCM_DEFAULT_DEVICE "default"
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_SAMPLE_BYTE_SIZE 2

// try and keep 100ms worth of audio in the Buffer
// at all times. This defines how active the audio
// thread is, and also will affect the latency
#define AUDIO_BUFFER_MS 100

#define MAX_AUDIO_POINTERS 5

// a sample 1m away = xxx milliseconds delay
#define METERS_TO_DELAY_MS 250

// defines an audio pointer to be sent to the sound output
// left_amount and right_amount are scalars, to be applied
// to the sample values going to the left and right channels
struct audio_pointer
{
  float left_amount;
  float right_amount;
  uint delayms;
  uint sound_index;
};

uint audio_pointers_count = 0;
audio_pointer audio_pointers[MAX_AUDIO_POINTERS];

std::thread audio_thread;

// array of arrays array containing our audio samples
#define SOUND_COUNT 5
int16_t *sound_arrays[SOUND_COUNT];
long sound_array_length[SOUND_COUNT];

#define SOUND_INDEX_CLAP 0
#define SOUND_INDEX_STARTUP 1
#define SOUND_INDEX_1BEEP 2
#define SOUND_INDEX_2BEEP 3
#define SOUND_INDEX_3BEEP 4

// ALSA global variables
snd_pcm_t *pcm_handle;
int16_t *alsa_buffer;
int alsa_buffer_length;
snd_pcm_uframes_t alsa_frames_length;

bool sound_ready;

void read_audio_file(char const *path, uint sound_index)
{
  assert(sound_index < SOUND_COUNT);

  FILE *fileptr;
  long filelen;
  const uint headerlen = 44;

  fileptr = fopen(path, "rb");
  // find the length of the file
  fseek(fileptr, 0, SEEK_END);
  filelen = ftell(fileptr) - headerlen;
  rewind(fileptr);
  // move past the .wav header
  fseek(fileptr, headerlen, SEEK_SET);
  sound_array_length[sound_index] = filelen + 1;
  sound_arrays[sound_index]  = (int16_t *)malloc(sound_array_length[sound_index]  * sizeof(int16_t));
  fread(sound_arrays[sound_index], filelen, 1, fileptr);
  fclose(fileptr);

  printw("Audio file read\n");
  printw("%i bytes not including header \n", sound_array_length[sound_index] );
  printw("%i milliseconds in length\n", sound_array_length[sound_index] * 1000 / AUDIO_SAMPLE_BYTE_SIZE / AUDIO_SAMPLE_RATE);
}

uint get_maximum_delayms()
{
  uint maxdelayms = 0;

  for (int i = 0; i < audio_pointers_count; i++)
  {
    if (audio_pointers[i].delayms > maxdelayms)
      maxdelayms = audio_pointers[i].delayms;
  }

  return maxdelayms;
}

uint get_delay_in_samples(uint delayms)
{
  uint delay = delayms * AUDIO_SAMPLE_RATE / 1000;

  return delay;
}

uint get_maximum_delay_samples()
{
  return get_delay_in_samples(get_maximum_delayms());
}

// gets a single byte sample. The samplei is the current sample index, delayms
// is the audio pointer delay to apply to samplei
int16_t get_sample(int samplei, uint delayms, uint sound_index)
{
  int16_t sample = 0;

  int samplei_delayed = samplei - get_delay_in_samples(delayms);

  if (samplei_delayed >= 0 && samplei_delayed < sound_array_length[sound_index])
    sample = sound_arrays[sound_index][samplei_delayed];

  return sample;
}

void audio_loop()
{
  unsigned int pcm = 0;
  int samplei = -1;
  snd_pcm_uframes_t frames_to_deliver;
  uint samples_to_deliver;
  sound_ready = false;

  assert(alsa_buffer_length % 4 == 0);

  if ((pcm = snd_pcm_prepare(pcm_handle)) < 0)
  {
    printw("cannot prepare audio interface for use (%s)\n", snd_strerror(pcm));
    refresh();
    exit(1);
  }

  while (1)
  {
    if (sound_ready)
    {
      sound_ready = false;

      samplei = 0;
    }

    uint samplebyte = 0;
    uint channel = 0;

    unsigned long delayms = 0;

    do
    {
      snd_pcm_sframes_t delayp;
      snd_pcm_delay(pcm_handle, &delayp);

      if (delayp > 0)
      {
        // delayp is the number of frames of IO delay
        // 1 frame = one sample period
        unsigned long delayms = delayp * 1000 / AUDIO_SAMPLE_RATE;

        if (delayms > AUDIO_BUFFER_MS)
        {
          usleep(AUDIO_BUFFER_MS * 500);
        }
      }
    } while (delayms > AUDIO_BUFFER_MS);

    if ((pcm = snd_pcm_wait(pcm_handle, 1000)) < 0)
    {
      printw("poll failed (%s)\n", strerror(errno));
      refresh();
      break;
    }

    // check the ALSA buffer to see how many frames it can accept
    if ((frames_to_deliver = snd_pcm_avail_update(pcm_handle)) < 0)
    {
      if (frames_to_deliver == -EPIPE)
      {
        printw("XRUN.\n");
        snd_pcm_prepare(pcm_handle);
        refresh();
        continue;
      }
      else
      {
        printw("UNKNOWN ERROR %d", pcm);
      }
      refresh();
    }

    // only deliver as much as can fit in our buffer
    if (frames_to_deliver > (alsa_buffer_length / 2))
    {
      frames_to_deliver = alsa_buffer_length / 2;
    }

    samples_to_deliver = frames_to_deliver * 2;

    // samples_to_deliver = alsa_buffer_length;

    // i represents the index into the interleaved ALSA buffer.
    // the alsa buffer is framed in groups of 2 16-bit integers:
    // [0 1]
    // 0: 16 bit left sample
    // 1: 16 bit right sample
    //
    // each loop iteration represents delivering a 16 bit sample to the left
    // and right ears.
    for (int i = 0; i < samples_to_deliver; i += 2)
    {
      if (i + 1 >= samples_to_deliver)
        break;

      int16_t sample16_left = 0;
      int16_t sample16_right = 0;

      if (samplei + 1 >= sound_array_length[1] + get_maximum_delay_samples())
      {
        samplei = -1;
      }

      if (samplei >= 0)
      {
        for (int pointeri = 0; pointeri < audio_pointers_count; pointeri++)
        {
          auto sample_delayms = audio_pointers[pointeri].delayms;
          auto sound_index = audio_pointers[pointeri].sound_index;

          int16_t this_sample16_left = get_sample(samplei, sample_delayms, sound_index);
          sample16_left += this_sample16_left * audio_pointers[pointeri].left_amount;

          if (sample16_left > 65535)
          {
            sample16_left = 65535;
            printw("CLIPPED\n");
            refresh();
          }

          int16_t this_sample16_right = get_sample(samplei, sample_delayms, sound_index);
          sample16_right += this_sample16_right * audio_pointers[pointeri].right_amount;
          if (sample16_right > 65535)
            sample16_right = 65535;
        }

        alsa_buffer[i] = sample16_left;
        alsa_buffer[i + 1] = sample16_right;

        samplei++;
      }
      else
      {
        alsa_buffer[i] = 0;
        alsa_buffer[i + 1] = 0;
      }
    }

    if (frames_to_deliver > 0)
    {
      if (pcm = snd_pcm_writei(pcm_handle, alsa_buffer, alsa_frames_length) == -EPIPE)
      {
        printw("XRUN.\n");
        snd_pcm_prepare(pcm_handle);
        refresh();
      }
      else if (pcm < 0)
      {
        printw("ERROR. Can't write to PCM device. %s\n", snd_strerror(pcm));
        refresh();
      }
    }
  }
}

int setup_audio(const char *device_name)
{
  unsigned int pcm, tmp, dir;
  unsigned int rate, channels;
  snd_pcm_hw_params_t *params;
  snd_pcm_sw_params_t *sw_params;

  rate = AUDIO_SAMPLE_RATE;
  channels = 2;

  /* Open the PCM device in playback mode */
  if (pcm = snd_pcm_open(&pcm_handle, device_name,
                         SND_PCM_STREAM_PLAYBACK, 0) < 0)
    printw("ERROR: Can't open \"%s\" PCM device. %s\n",
           device_name, snd_strerror(pcm));

  /* Allocate parameters object and fill it with default values*/
  snd_pcm_hw_params_malloc(&params);

  snd_pcm_hw_params_any(pcm_handle, params);

  /* Set parameters */
  if (pcm = snd_pcm_hw_params_set_access(pcm_handle, params,
                                         SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
    printw("ERROR: Can't set interleaved mode. %s\n", snd_strerror(pcm));

  if (pcm = snd_pcm_hw_params_set_format(pcm_handle, params,
                                         SND_PCM_FORMAT_S16_LE) < 0)
    printw("ERROR: Can't set format. %s\n", snd_strerror(pcm));

  if (pcm = snd_pcm_hw_params_set_channels(pcm_handle, params, channels) < 0)
    printw("ERROR: Can't set channels number. %s\n", snd_strerror(pcm));

  if (pcm = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0) < 0)
    printw("ERROR: Can't set rate. %s\n", snd_strerror(pcm));

  /* Write parameters */
  if (pcm = snd_pcm_hw_params(pcm_handle, params) < 0)
    printw("ERROR: Can't set hardware parameters. %s\n", snd_strerror(pcm));

  /* Resume information */
  printw("PCM name: '%s'\n", snd_pcm_name(pcm_handle));

  printw("PCM state: %s\n", snd_pcm_state_name(snd_pcm_state(pcm_handle)));

  snd_pcm_hw_params_get_channels(params, &tmp);

  if (tmp == 1)
    printw("(mono)\n");
  else if (tmp == 2)
    printw("(stereo)\n");

  /* Allocate buffer to hold single period */
  snd_pcm_hw_params_get_period_size(params, &alsa_frames_length, 0);
 
  printw("frames: %lu\n", alsa_frames_length);
  alsa_buffer_length = alsa_frames_length * channels; /* 2 -> sample size */

  alsa_buffer = (int16_t *)malloc(alsa_buffer_length * sizeof(int16_t));
  printw("Buffer size: %lu\n", alsa_buffer_length);

  printw("Starting audio thread \n");
  refresh();

  audio_thread = std::thread(&audio_loop);

  return 0;
}