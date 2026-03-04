#include "base/Logger.h"
#include "network/net/Eventloop.h"
#include "media/rtmp/RtmpServer.h"
#include <csignal>

using namespace tms;
using namespace tms::network;
using namespace tms::media;

int main()
{
    base::InitLogger();
    LOG_INFO("═══════════════════════════════════════");
    LOG_INFO("  TMS - Tiny Media Server v0.1.0");
    LOG_INFO("═══════════════════════════════════════");

    try
    {
        Eventloop base_loop;
        RtmpServer rtmp_server(&base_loop, 1935);
        rtmp_server.SetThreadNum(4);
        rtmp_server.Start();

        LOG_INFO("test: ffmpeg -re -f lavfi -i testsrc=size=320x240:rate=15 "
                 "-c:v libx264 -f flv rtmp://localhost:1935/live/test");
        LOG_INFO("Ctrl+C to quit");

        asio::signal_set signals(base_loop.IoContext(), SIGINT, SIGTERM);
        signals.async_wait([&](asio::error_code, int sig)
        {
            LOG_INFO("signal {}, shutting down....", sig);
            rtmp_server.Stop();
            base_loop.Quit();
        });

        base_loop.Loop();
    }
    catch (std::exception& e)
    {
        LOG_ERROR("exception: {}", e.what());
        return 1;
    }

    LOG_INFO("server stopped");
    return 0;
}