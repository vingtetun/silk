#ifndef SIMPLEH264ENCODER_H
#define SIMPLEH264ENCODER_H
/**
 * An H264 encoder with as few user-serviceable parts as possible
 */

namespace android {
class MediaBuffer;
}

class SimpleH264Encoder {
 public:
  struct EncodedFrameInfo {
    void *userData;
    void *encodedFrame;
    int encodedFrameLength;
    bool keyFrame;
    long long timeMillis;
  };

  // Callback that receives the next encoded frame.
  // This is not your thread or data. Copy and return.
  typedef void (*FrameOutCallback)(EncodedFrameInfo& info);
  static SimpleH264Encoder *Create(int width,
                                   int height,
                                   int maxBitrateK,
                                   int targetFps,
                                   FrameOutCallback frameOutCallback,
                                   void *frameOutUserData);

  virtual ~SimpleH264Encoder() {};
  virtual void setBitRate(int bitrateK) = 0;
  virtual void requestKeyFrame() = 0;
  virtual void nextFrame(void *yuv420SemiPlanarFrame, long long timestamp, void (*deallocator)(void *)) = 0;
  virtual void nextFrame(android::MediaBuffer *yuv420SemiPlanarFrame) = 0;
  virtual void stop() = 0;
};

#endif
