
#include <binder/ProcessState.h>
#include <cutils/properties.h>
#include <utils/SystemClock.h>
#include <fcntl.h>

#include "libpreview.h"
#include "SimpleH264Encoder.h"

using android::Mutex;

libpreview::Client *libpreviewClient;
SimpleH264Encoder *simpleH264Encoder;
Mutex simpleH264EncoderLock;
int fd;

void libpreview_FrameCallback(void *userData,
                              void *frame,
                              libpreview::FrameFormat format,
                              size_t width,
                              size_t height,
                              libpreview::FrameOwner owner)
{
  Mutex::Autolock autolock(simpleH264EncoderLock);

  if (simpleH264Encoder == nullptr) {
    libpreviewClient->releaseFrame(owner);
    return;
  }

  if (format == libpreview::FRAMEFORMAT_YVU420SP) {
    const size_t size = height * width * 3 / 2;

    uint8_t *data = (uint8_t *) malloc(size);
    memcpy(data, frame, width * height);

    // Convert from YVU420SemiPlaner to YUV420SemiPlaner
    uint8_t *s = static_cast<uint8_t *>(frame) + width * height;
    uint8_t *d = data + width * height;
    uint8_t *dEnd = d + width * height / 2;
    for (; d < dEnd; s += 2, d += 2) {
      d[0] = s[1];
      d[1] = s[0];
    }

    libpreviewClient->releaseFrame(owner);
    simpleH264Encoder->nextFrame(data, android::elapsedRealtime(), free);
  }
}

void libpreview_AbandonedCallback(void *userData)
{
  printf("libpreview_AbandonedCallback\n");
  exit(1);
}

void frameOutCallback(SimpleH264Encoder::EncodedFrameInfo& info) {
  printf("Frame %lld size=%8d keyframe=%d\n",
    info.timeMillis, info.encodedFrameLength, info.keyFrame);
  TEMP_FAILURE_RETRY(
    write(
      fd,
      info.encodedFrame,
      info.encodedFrameLength
    )
  );
}

int main(int argc, char **argv)
{
  android::sp<android::ProcessState> ps = android::ProcessState::self();
  ps->startThreadPool();

  libpreviewClient = libpreview::open(libpreview_FrameCallback, libpreview_AbandonedCallback, 0);
  if (!libpreviewClient) {
    printf("Unable to open libpreview\n");
    return 1;
  }

  int width = property_get_int32("ro.silk.camera.width", 1280);
  int height = property_get_int32("ro.silk.camera.height", 720);
  int vbr = property_get_int32("ro.silk.camera.vbr", 1024);
  int fps = property_get_int32("ro.silk.camera.fps", 24);

  for (int i = 0; i < 5; i++) {
    char filename[32];
    snprintf(filename, sizeof(filename) - 1, "/data/vid_%d.h264", i);
    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0440);
    if (fd < 0) {
      printf("Unable to open output file: %s\n", filename);
    }

    printf("Output file: %s\n", filename);
    {
      Mutex::Autolock autolock(simpleH264EncoderLock);
      simpleH264Encoder = SimpleH264Encoder::Create(
        width, height, vbr, fps,
        frameOutCallback,
        nullptr
      );
    }
    printf("Encoder started\n");
    if (simpleH264Encoder == nullptr) {
      printf("Unable to create a SimpleH264Encoder\n");
      return 1;
    }

    // Fiddle with the bitrate while recording just because we can
    for (int j = 0; j < 10; j++) {
      int bitRateK = 1000 * (j+1) / 10;
      simpleH264Encoder->setBitRate(bitRateK);
      printf(". (bitrate=%dk)\n", bitRateK);
      sleep(1);
    }

    simpleH264Encoder->stop();
    {
      Mutex::Autolock autolock(simpleH264EncoderLock);
      delete simpleH264Encoder;
      simpleH264Encoder = nullptr;
    }
    close(fd);
    printf("Encoder stopped\n");
    sleep(1); // Take a breath...
  }

  printf("Releasing libpreview\n");
  libpreviewClient->release();

  return 0;
}
