#pragma once
#include <media/NdkImageReader.h>
#include <media/NdkImage.h>
#include <android/native_window.h>

class ImageReaderHandler
{
    public:
        ImageReaderHandler();
        ~ImageReaderHandler();

        bool InitReader(int32_t width, int32_t height);
        void ReleaseReader();

        ANativeWindow* GetWindow() const;
        bool SetImageListener(AImageReader_ImageListener* listener);

    private:
        AImageReader* reader_;
        ANativeWindow* readerWindow_;
        int32_t width_; 
        int32_t height_;
};