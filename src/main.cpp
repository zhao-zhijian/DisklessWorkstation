#include <iostream>
#include <thread>
#include <chrono>
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/magnet_uri.hpp>

int main()
{
    try
    {
        std::cout << "=== LibTorrent 2.0.10 Simple Example ===" << std::endl;
        std::cout << "LibTorrent Version: " << LIBTORRENT_VERSION << std::endl;
        std::cout << std::endl;

        // Create session parameters
        lt::session_params params;
        params.settings.set_int(lt::settings_pack::alert_mask,
            lt::alert_category::error | lt::alert_category::status);

        // Create session
        lt::session ses(params);

        std::cout << "Session created successfully" << std::endl;
        std::cout << "Listening port: " << ses.listen_port() << std::endl;
        std::cout << std::endl;

        // Show current torrent count
        std::vector<lt::torrent_handle> torrents = ses.get_torrents();
        std::cout << "Current torrent count: " << torrents.size() << std::endl;
        std::cout << std::endl;

        // Example: Parse magnet link (without actual download)
        std::cout << "=== Magnet Link Parse Example ===" << std::endl;
        std::string magnet_uri = "magnet:?xt=urn:btih:1234567890123456789012345678901234567890";
        
        try
        {
            lt::add_torrent_params atp = lt::parse_magnet_uri(magnet_uri);
            std::cout << "Magnet link parsed successfully" << std::endl;
            if (atp.info_hashes.has_v1())
            {
                std::cout << "Info Hash v1: " << atp.info_hashes.v1 << std::endl;
            }
            if (atp.info_hashes.has_v2())
            {
                std::cout << "Info Hash v2: " << atp.info_hashes.v2 << std::endl;
            }
        }
        catch (const std::exception& e)
        {
            std::cout << "Magnet link parse failed (this is normal for example link): " << e.what() << std::endl;
        }
        std::cout << std::endl;

        // Wait for a while to process alerts
        std::cout << "Waiting 2 seconds to process alerts..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Process alerts
        std::vector<lt::alert*> alerts;
        ses.pop_alerts(&alerts);
        
        if (!alerts.empty())
        {
            std::cout << "Received " << alerts.size() << " alerts" << std::endl;
            for (auto* alert : alerts)
            {
                std::cout << "  - [" << alert->what() << "] " << alert->message() << std::endl;
            }
        }
        else
        {
            std::cout << "No alerts received" << std::endl;
        }
        std::cout << std::endl;

        std::cout << "=== Example Completed ===" << std::endl;
        std::cout << "LibTorrent is working correctly!" << std::endl;

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

