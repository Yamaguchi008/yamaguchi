#include <SDHCI.h>
#include <Audio.h>
#include <EEPROM.h>
#include <audio/utilities/playlist.h>
#include <stdio.h>
#include <stdlib.h>

#include <BMI160Gen.h>
#include <MemoryUtil.h>
#include <SensorManager.h>
#include <AccelSensor.h>
#include <Aesm.h>
#include <ApplicationSensor.h>

SDClass theSD;
AudioClass *theAudio;
Playlist thePlaylist1("TRACK_D1.CSV");
Playlist thePlaylist2("TRACK_D2.CSV");
Playlist thePlaylist3("TRACK_D3.CSV");
Playlist thePlaylist4("TRACK_D4.CSV");
Playlist* thePlaylist = &thePlaylist1;

Track currentTrack;

int eeprom_idx = 0;
struct SavedObject {
  int saved;
  int volume;
  int random;
  int repeat;
  int autoplay;
} preset;

const int i2c_addr                = 0x68;
const int baudrate                = 115200;

const int walking_stride          = 60; /* 60cm */
const int running_stride          = 80; /* 80cm */

const int accel_range             =  2; /* 2G */
const int accel_rate              = 50; /* 50 Hz */
const int accel_sample_num        = 50; /* 50 sample */
const int accel_sample_size       = sizeof(float) * 3;

float counter;
int kilometer;
float b_counter;
int difference;
int command;
int total;

const int buttonPin = 13;  // the number of the pushbutton pin
const int ledPin    = 12;  // the number of the led pin
int button = 0;

File myFile;

bool ErrEnd = false;

static void menu()
{
  printf("=== MENU (input key ?) ==============\n");
  printf("p: play  s: stop  +/-: volume up/down\n");
  printf("l: list  n: next  b: back\n");
  printf("?: menu\n");
  printf("=====================================\n");
}

bool step_counter_result(sensor_command_data_mh_t &data)
{
  /* Display result of sensing */

  StepCounterStepInfo* steps =
              reinterpret_cast<StepCounterStepInfo*>
              (StepCountReader.subscribe(data));
  
  if (steps == NULL)
    {
      return 0;
    }

  float  tempo = 0;

  switch (steps->movement_type)
    {
      case STEP_COUNTER_MOVEMENT_TYPE_WALK:
      case STEP_COUNTER_MOVEMENT_TYPE_RUN:
        tempo = steps->tempo;
        break;
      case STEP_COUNTER_MOVEMENT_TYPE_STILL:

        /* In this state, the tempo value on the display is zero. */

        tempo = 0;

        break;
      default:

        /* It is not displayed in the state other than the above. */

        return 0;
    }

  printf("%11.5f,%11.2f,%11.5f,%11.5f,%11ld,",
               tempo,
               steps->stride,
               steps->speed,
               steps->distance,
               steps->step);

  switch (steps->movement_type)
    {
      case STEP_COUNTER_MOVEMENT_TYPE_STILL:
        puts("   stopping");
        break;
      case STEP_COUNTER_MOVEMENT_TYPE_WALK:
        puts("   walking");
        break;
      case STEP_COUNTER_MOVEMENT_TYPE_RUN:
        puts("   running");
        break;
      default:
        puts("   UNKNOWN");
        break;
    }

   b_counter = counter;
   counter = (steps->distance)/10;
   difference = (int)counter-(int)b_counter;

  if(difference!=0){
    total = (int)counter;
    }else{
      total = 0;
      }
   
   kilometer = (int)counter;
   
//   printf("%11.5f,%11.5f,%11.5f,%d,%d\n",steps->distance,counter,b_counter,difference,total);

  return 0;
}

static void show(Track *t)
{
  printf("%s | %s | %s\n", t->author, t->album, t->title);
}

static void list()
{
  Track t;
  thePlaylist->restart();
  printf("-----------------------------\n");
  while (thePlaylist->getNextTrack(&t)) {
    if (0 == strncmp(currentTrack.title, t.title, 64)) {
      printf("-> ");
    }
    show(&t);
  }
  printf("-----------------------------\n");

  /* restore the current track */
  thePlaylist->restart();
  while (thePlaylist->getNextTrack(&t)) {
    if (0 == strncmp(currentTrack.title, t.title, 64)) {
      break;
    }
  }
}

static bool next()
{
  if (thePlaylist->getNextTrack(&currentTrack)) {
    show(&currentTrack);
    return true;
  } else {
    return false;
  }
}

static bool prev()
{
  if (thePlaylist->getPrevTrack(&currentTrack)) {
    show(&currentTrack);
    return true;
  } else {
    return false;
  }
}

static err_t setPlayer(Track *t)
{
  static uint8_t   s_codec   = 0;
  static uint32_t  s_fs      = 0;
  static uint8_t   s_bitlen  = 0;
  static uint8_t   s_channel = 0;
  static AsClkMode s_clkmode = (AsClkMode)-1;
  err_t err = AUDIOLIB_ECODE_OK;
  AsClkMode clkmode;

  if ((s_codec   != t->codec_type) ||
      (s_fs      != t->sampling_rate) ||
      (s_bitlen  != t->bit_length) ||
      (s_channel != t->channel_number)) {

    /* Set audio clock 48kHz/192kHz */

    clkmode = (t->sampling_rate <= 48000) ? AS_CLKMODE_NORMAL : AS_CLKMODE_HIRES;

    if (s_clkmode != clkmode) {

      /* When the audio master clock will be changed, it should change the clock
       * mode once after returning the ready state. At the first start-up, it
       * doesn't need to call setReadyMode().
       */

      if (s_clkmode != (AsClkMode)-1) {
        theAudio->setReadyMode();
      }

      s_clkmode = clkmode;

      theAudio->setRenderingClockMode(clkmode);

      theAudio->setPlayerMode(AS_SETPLAYER_OUTPUTDEVICE_SPHP, AS_SP_DRV_MODE_LINEOUT);
    }

    /* Initialize player */

    err = theAudio->initPlayer(AudioClass::Player0,
                               t->codec_type,
                               "/mnt/sd0/BIN",
                               t->sampling_rate,
                               t->bit_length,
                               t->channel_number);
    if (err != AUDIOLIB_ECODE_OK) {
      printf("Player0 initialize error\n");
      return err;
    }

    s_codec   = t->codec_type;
    s_fs      = t->sampling_rate;
    s_bitlen  = t->bit_length;
    s_channel = t->channel_number;
  }

  return err;
}

static bool start()
{
  err_t err;
  Track *t = &currentTrack;

  /* Prepare for playback with the specified track */

  err = setPlayer(t);

  if (err != AUDIOLIB_ECODE_OK) {
    return false;
  }

  /* Open file placed on SD card */

  char fullpath[64] = { 0 };
  snprintf(fullpath, sizeof(fullpath), "AUDIO/%s", t->title);

  myFile = theSD.open(fullpath);

  if (!myFile){
    printf("File open error\n");
    return false;
  }

  /* Send first frames to be decoded */

  err = theAudio->writeFrames(AudioClass::Player0, myFile);

  if ((err != AUDIOLIB_ECODE_OK) && (err != AUDIOLIB_ECODE_FILEEND)) {
    printf("File Read Error! =%d\n",err);
    return false;
  }

  printf("start\n");
  theAudio->startPlayer(AudioClass::Player0);

  return true;
}

static void stop()
{
  printf("stop\n");
  theAudio->stopPlayer(AudioClass::Player0, AS_STOPPLAYER_NORMAL);
  myFile.close();
}

static void audio_attention_cb(const ErrorAttentionParam *atprm)
{
  puts("Attention!");

  if (atprm->error_code >= AS_ATTENTION_CODE_WARNING) {
    ErrEnd = true;
  }
}

void setup()
{
  const char *playlist_dirname = "/mnt/sd0/PLAYLIST";
  bool success;

  /* Initialize device. */
  BMI160.begin(BMI160GenClass::I2C_MODE, i2c_addr);

  /* Set device setting */
  BMI160.setAccelerometerRange(accel_range);
  BMI160.setAccelerometerRate(accel_rate);

  /* Display menu */
  Serial.begin(115200);
  menu();

  /* Load preset data */
  EEPROM.get(eeprom_idx, preset);
  if (!preset.saved) {
    /* If no preset data, come here */
    preset.saved = 1;
    preset.volume = -160; /* default */
    preset.random = 0;
    preset.repeat = 0;
    preset.autoplay = 0;
    EEPROM.put(eeprom_idx, preset);
  }
  printf("Volume=%d\n", preset.volume);
  printf("Random=%s\n", (preset.random) ? "On" : "Off");
  printf("Repeat=%s\n", (preset.repeat) ? "On" : "Off");
  printf("Auto=%s\n", (preset.autoplay) ? "On" : "Off");

  /* Initialize SD */
  while (!theSD.begin())
    {
      Serial.println("Insert SD card.");
    }

  /* Initialize playlist */
  success = thePlaylist1.init(playlist_dirname);
  success = thePlaylist2.init(playlist_dirname);
  success = thePlaylist3.init(playlist_dirname);
  success = thePlaylist4.init(playlist_dirname);
  if (!success) {
    printf("ERROR: no exist playlist file %s/TRACK_DB.CSV\n",
           playlist_dirname);
    while (1);
  }

  /* Set random seed to use shuffle mode */
  struct timespec ts;
  clock_systimespec(&ts);
  srand((unsigned int)ts.tv_nsec);

  /* Restore preset data */
  if (preset.random) {
    thePlaylist1.setPlayMode(Playlist::PlayModeShuffle);
    thePlaylist2.setPlayMode(Playlist::PlayModeShuffle);
    thePlaylist3.setPlayMode(Playlist::PlayModeShuffle);
    thePlaylist4.setPlayMode(Playlist::PlayModeShuffle);
  }

  if (preset.repeat) {
    thePlaylist1.setRepeatMode(Playlist::RepeatModeOn);
    thePlaylist2.setRepeatMode(Playlist::RepeatModeOn);
    thePlaylist3.setRepeatMode(Playlist::RepeatModeOn);
    thePlaylist4.setRepeatMode(Playlist::RepeatModeOn);
  }

  thePlaylist1.getNextTrack(&currentTrack);
  thePlaylist2.getNextTrack(&currentTrack);
  thePlaylist3.getNextTrack(&currentTrack);
  thePlaylist4.getNextTrack(&currentTrack);

  if (preset.autoplay) {
    show(&currentTrack);
  }

  /* Start audio system */
  theAudio = AudioClass::getInstance();
  theAudio->begin(audio_attention_cb);

  /* Set main volume */
  theAudio->setVolume(preset.volume);

  SensorManager.begin();

  AccelSensor.begin(SEN_accelID,
                    accel_rate,
                    accel_sample_num,
                    accel_sample_size);

  Aesm.begin(SEN_stepcounterID,
             SUBSCRIPTION(SEN_accelID),
             accel_rate,
             accel_sample_num,
             accel_sample_size);

  StepCountReader.begin(SEN_app0ID,
                        SUBSCRIPTION(SEN_stepcounterID),
                        step_counter_result);

  /* Initialize StepCounter parameters */
  Aesm.set(walking_stride, running_stride);

  puts("Start sensing...");
  puts("-----------------------------------------------------------------------");
  puts("      tempo,     stride,      speed,   distance,       step,  move-type");

  // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT_PULLUP);  
  // initialize the LED pin as an output:
  pinMode(LED0, OUTPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); 

}

void loop()
{
  float x;
  float y;
  float z;
  int CHECK = 1;

  BMI160.readAccelerometerScaled(x, y, z);
  AccelSensor.write_data(x, y, z);

  int buttonState = digitalRead(buttonPin);
  if (buttonState == LOW) {
   digitalWrite(LED0, HIGH);
   digitalWrite(ledPin, HIGH);
   button = 1;
  } else {
   digitalWrite(LED0, LOW);
   digitalWrite(ledPin, LOW); 
   button = 0;
  }
  
  static enum State {
    Stopped,
    Ready,
    Active
  } s_state = preset.autoplay ? Ready : Stopped;
  err_t err = AUDIOLIB_ECODE_OK;

  /* Fatal error */
  if (ErrEnd) {
    printf("Error End\n");
    goto stop_player;
  }

  /* Menu operation */  
    switch (total) {  
    case 0: // change list
//      if (s_state == Active) {
//        stop();
//        s_state = Ready;
//      }
      thePlaylist = &thePlaylist3;
      break;
      
    case 1: // change list
      if (s_state == Active) {
        stop();
        s_state = Ready;
      }
      prev();
      list();
      thePlaylist = &thePlaylist1;
      break;
      
    case 2: // change list
      if (s_state == Active) {
        stop();
        s_state = Ready;
      }
      prev();
      list();      
      thePlaylist = &thePlaylist2;
      break;
      
    case 3: // change list
      if (s_state == Active) {
        stop();
        s_state = Ready;
      }
      prev();
      list();     
      thePlaylist = &thePlaylist3;
      break;

    case 4: // change list
      if (s_state == Active) {
        stop();
        s_state = Ready;
      }
      prev();
      list();      
      thePlaylist = &thePlaylist4;
      break;   
    }

    switch (button) {  
    case 1: // play
      if (s_state == Stopped) {
        s_state = Ready;
        show(&currentTrack);
      }
      break;
    }
 
  if (Serial.available() > 0) {
    switch (Serial.read()) {  
    case 'p': // play
      if (s_state == Stopped) {
        s_state = Ready;
        show(&currentTrack);
      }
      break;
      
    case 's': // stop
      if (s_state == Active) {
        stop();
      }
      s_state = Stopped;
      break;
      
    case '+': // volume up
      preset.volume += 10;
      if (preset.volume > 120) {
        /* set max volume */
        preset.volume = 120;
      }
      printf("Volume=%d\n", preset.volume);
      theAudio->setVolume(preset.volume);
      EEPROM.put(eeprom_idx, preset);
      break;
      
    case '-': // volume down
      preset.volume -= 10;
      if (preset.volume < -1020) {
        /* set min volume */
        preset.volume = -1020;
      }
      printf("Volume=%d\n", preset.volume);
      theAudio->setVolume(preset.volume);
      EEPROM.put(eeprom_idx, preset);
      break;
      
    case 'l': // list
      if (preset.repeat) {
        thePlaylist->setRepeatMode(Playlist::RepeatModeOff);
        list();
        thePlaylist->setRepeatMode(Playlist::RepeatModeOn);
      } else {
        list();
      }
      break;
      
    case 'n': // next
      if (s_state == Ready) {
        // do nothing
      } else { // s_state == Active or Stopped
        if (s_state == Active) {
          stop();
          s_state = Ready;
        }
        if (!next()) {
          s_state = Stopped;
        }
      }
      break;
      
    case 'b': // back
      if (s_state == Active) {
        stop();
        s_state = Ready;
      }
      prev();
      break;
      
    case '?':
      menu();
      break;
    default:
      break;
    }
  }

  /* Processing in accordance with the state */

  switch (s_state) {
  case Stopped:
    break;

  case Ready:
    if (start()) {
      s_state = Active;
    } else {
      goto stop_player;
    }
    break;

  case Active:
    /* Send new frames to be decoded until end of file */

    err = theAudio->writeFrames(AudioClass::Player0, myFile);

    if (err == AUDIOLIB_ECODE_FILEEND) {
      /* Stop player after playback until end of file */

      theAudio->stopPlayer(AudioClass::Player0, AS_STOPPLAYER_ESEND);
      myFile.close();
      if (next()) {
        s_state = Ready;
      } else {
        /* If no next track, stop the player */
        s_state = Stopped;
      }
    } else if (err != AUDIOLIB_ECODE_OK) {
      printf("Main player error code: %d\n", err);
      goto stop_player;
    }
    break;

  default:
    break;
  }

  usleep(1000);

  return;

stop_player:
  printf("Exit player\n");
  myFile.close();
  exit(1);
}
