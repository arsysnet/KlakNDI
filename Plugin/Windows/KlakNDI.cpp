#include <thread>
#include <Processing.NDI.Lib.h>
#include "Unity/IUnityInterface.h"

namespace
{
    // NDI global observer class
    class Observer
    {
        public:

            // Get the singleton instance.
            static Observer& getInstance()
            {
                static Observer instance;
                return instance;
            }

            // Initialize and start observing.
            void Start()
            {
                // If the previous instance is still running, wait for its termination.
                if (finderThread_.joinable())
                {
                    finderShouldStop_ = true;
                    finderThread_.join();
                }

                // Start a new observer thread.
                finderShouldStop_ = false;
                finderThread_ = std::thread(&Observer::ObserverThread, this);
            }

            // Request stopping the observer.
            void Stop()
            {
                finderShouldStop_ = true;
            }

			// Create a receiver instance with a found source.
			NDIlib_recv_instance_t createReceiverWithFoundSource() const
			{
				uint32_t count;
				auto sources = NDIlib_find_get_current_sources(finder_, &count);
				if (count == 0) return nullptr;
				return NDIlib_recv_create_v2(&NDIlib_recv_create_t(sources[0]));
			}

        private:

            NDIlib_find_instance_t finder_;

            std::thread finderThread_;
            bool finderShouldStop_;

            void ObserverThread()
            {
                NDIlib_initialize();

                finder_ = NDIlib_find_create_v2(&NDIlib_find_create_t());
                if (finder_ == nullptr) return;

                while (!finderShouldStop_)
                    NDIlib_find_wait_for_sources(finder_, 100);

                NDIlib_find_destroy(finder_);
                finder_ = nullptr;

                NDIlib_destroy();
            }
    };

	NDIlib_video_frame_v2_t receiver_frame;
}

// Global functions

extern "C" void UNITY_INTERFACE_EXPORT NDI_Initialize()
{
    Observer::getInstance().Start();
}

extern "C" void UNITY_INTERFACE_EXPORT NDI_Finalize()
{
    Observer::getInstance().Stop();
}

// Sender functions

extern "C" void UNITY_INTERFACE_EXPORT *NDI_CreateSender(const char* name)
{
    NDIlib_send_create_t desc;
    desc.p_ndi_name = name;
    return NDIlib_send_create(&desc);
}

extern "C" void UNITY_INTERFACE_EXPORT NDI_DestroySender(void* sender)
{
    NDIlib_send_destroy(sender);
}

extern "C" void UNITY_INTERFACE_EXPORT NDI_SendFrame(void* sender, void* data, int width, int height)
{
    NDIlib_video_frame_v2_t frame;

    frame.xres = width;
    frame.yres = height;
    frame.FourCC = NDIlib_FourCC_type_BGRX;
    frame.frame_format_type = NDIlib_frame_format_type_interleaved;
    frame.frame_rate_N = 60;
    frame.frame_rate_D = 1;
    frame.p_data = static_cast<uint8_t*>(data);
    frame.line_stride_in_bytes = width * 4;

    NDIlib_send_send_video_v2(sender, &frame);
}

// Receiver functions

extern "C" void UNITY_INTERFACE_EXPORT *NDI_CreateReceiver()
{
	return Observer::getInstance().createReceiverWithFoundSource();
}

extern "C" void UNITY_INTERFACE_EXPORT NDI_DestroyReceiver(void* receiver)
{
	NDIlib_recv_destroy(receiver);
}

extern "C" bool UNITY_INTERFACE_EXPORT NDI_ReceiveFrame(void* receiver)
{
	auto type = NDIlib_recv_capture_v2(receiver, &receiver_frame, nullptr, nullptr, 0);
	return type == NDIlib_frame_type_video;
}

extern "C" void UNITY_INTERFACE_EXPORT NDI_FreeFrame(void* receiver)
{
	NDIlib_recv_free_video_v2(receiver, &receiver_frame);
}

extern "C" int UNITY_INTERFACE_EXPORT NDI_GetFrameWidth(void* receiver)
{
    return receiver_frame.xres;
}

extern "C" int UNITY_INTERFACE_EXPORT NDI_GetFrameHeight(void* receiver)
{
    return receiver_frame.yres;
}

extern "C" void UNITY_INTERFACE_EXPORT *NDI_GetFrameData(void* receiver)
{
    return receiver_frame.p_data;
}