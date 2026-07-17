#include "image_reader_handler.h"
#include <android/hardware_buffer.h>

ImageReaderHandler::ImageReaderHandler()
    : reader_(nullptr), readerWindow_(nullptr), width_(0), height_(0) {}

ImageReaderHandler::~ImageReaderHandler() { ReleaseReader(); }

bool ImageReaderHandler::InitReader(int32_t width, int32_t height) {
  width_ = width;
  height_ = height;

  media_status_t status =
      AImageReader_newWithUsage(width_, height_, AIMAGE_FORMAT_YUV_420_888,
                                AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                                    AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER |
                                    AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN,
                                3, &reader_);

  if (status != AMEDIA_OK || reader_ == nullptr) {
    return false;
  }

  status = AImageReader_getWindow(reader_, &readerWindow_);
  if (status != AMEDIA_OK || readerWindow_ == nullptr) {
    ReleaseReader();
    return false;
  }

  return true;
}

void ImageReaderHandler::ReleaseReader() {
  if (reader_ != nullptr) {
    AImageReader_delete(reader_);
    reader_ = nullptr;
  }

  readerWindow_ = nullptr;
  width_ = 0;
  height_ = 0;
}

ANativeWindow *ImageReaderHandler::GetWindow() const { return readerWindow_; }

bool ImageReaderHandler::SetImageListener(
    AImageReader_ImageListener *listener) {
  if (reader_ == nullptr) {
    return false;
  }

  media_status_t status = AImageReader_setImageListener(reader_, listener);
  return (status == AMEDIA_OK);
}