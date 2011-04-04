/*
 * This is a simple test HTTP server that responds with the http request in the body of the message.
 * Could be potentially useful as a tool for debugging browsers. :)
 */

#include "clientserver/clientserver.h"
#include "httpd/httpd.h"
#include "util/util.h"


void process(const httpd::request& req, httpd::responder& resp) {

    bool json = false;

    // Just to show an easy way at the get-parameters.
    if (req.get_query("json") == "1") {
        json = true;
    }

    // All http parameters are parsed and easily accessible.

    if (json) {
        // Yes, this isn't really JSON. :)
        resp << "{ \n"
             << "  'version':   '" << req.version << "'\n"
             << "  'method':    '" << req.method << "'\n"
             << "  'path':      '" << req.path << "'\n"
             << "  'raw_query': '" << req.query_raw << "'\n"
             << "  'queries': {\n";
        
        for (httpd::request::queries_::const_iterator i = req.queries.begin(); i != req.queries.end(); ++i) {
            resp << "    '" << i->first << "': [";
            for (std::vector<std::string>::const_iterator j = i->second.begin(); j != i->second.end(); ++j) {
                resp << " '" << *j << "'";
            }
            resp << " ],\n";
        }

        resp << "}}\n";

    } else {

        resp << req.version << '\n' << req.method << '\n'
             << req.path << '?' << req.query_raw << '\n'
             << req.fields_raw << "\n\n"
             << httpd::unparse_request(req);
    }    

    resp.set_field("content-type", (json ? "application/json" : "text/plain"));
    resp.code = "200 OK";
}


void service(clientserver::service_buffer sock) {

    while (1) {

        httpd::request req;

        try {
            httpd::parse_request(sock, req);

        } catch (clientserver::eof_exception& e) {
            /* ... */
            return;

        } catch (std::exception& e) {
            logger::log(logger::ERROR) << "ERROR in reading client request: " << e.what();
            return;

        } catch (...) {
            logger::log(logger::ERROR) << "ERROR in reading client request: unknown error.";
            return;
        }


        logger::log(logger::IN) << "GET " << req.path << "?" << req.query_raw;

        httpd::responder resp(sock, req);

        try {

            process(req, resp);

            resp.send();

        } catch (std::exception& e) {
            resp.code = "503 Service Unavailable";
            logger::log(logger::ERROR) << "ERROR in writing response: " << e.what();

        } catch (...) {
            resp.code = "503 Service Unavailable";
            logger::log(logger::ERROR) << "ERROR in writing response: unknown error.";
        }

        // HTTP/1.0: Закрываем соединение.
        if (resp.should_close) break;
    }
}



int main(int argc, char** argv) {

    try {

        std::string host = "0.0.0.0";
        unsigned int port = 9876;
        std::string info_log = "example.log";
        std::string error_log = "example.err";
        std::string pidfile = "example.pid";
        bool do_daemon = true;

        while (1) {
            int c = ::getopt(argc, argv, "d:h:p:i:e:P:");

            if (c == -1) 
                break;

            switch (c) {
            case 'h':
                host = optarg;
                break;

            case 'p':
                port = ::atoi(optarg);
                break;

            case 'i':
                info_log = optarg;
                break;

            case 'e':
                error_log = optarg;
                break;

            case 'P':
                pidfile = optarg;
                break;

            case 'd':
                do_daemon = ::atoi(optarg);
                break;

            default:
                std::cout << "Usage: http_test -h <local_ip> -p <local_port> -i <info logfile> -e <error logfile> "
                          << "-P <pidfile> -d <do_daemonize, 1 or 0>"
                          << std::endl;
                return 1;
            }
        }


        util::daemonize(pidfile, info_log, error_log, do_daemon);

        clientserver::server_socket server(host, port);

        // Watch out, this is important!!
        // The first two arguments are self-explanatory: a server socket object and a function object for calbacks.
        //
        // The other two are less obvious.
        // 
        // The third is 'maxconns', a maximum limit on the number of simultaneous connections.
        // It is best set to a large value, like 1000; there is no harm in keeping connections active, and this 
        // parameter is only useful as a measure against accidental or purposeful DDOS attacks.
        //
        // The fourth is 'priority', a boolean. Default is 'false', which means that the server will spawn
        // regular old threads to process requests.
        // A value of 'true' will cause the server to spawn real-time threads, which should balance load
        // much more smoothly under real production loads.
        //
        // WARNING: Support for real-time threads must be enabled beforehand, via the shell:
        //
        //     $ ulimit -r 1
        // 

        clientserver::serve(server, service, 1000, false);

        logger::log(logger::INFO, true) << "Started server thread.";

        while (1) {
            ::sleep(60);

            // Nothing to do in the main thread. Add your own. :)
        }

    } catch (std::exception& e) {

        logger::log(logger::FATAL) <<  e.what();
        return 1;

    } catch (...) {

        logger::log(logger::FATAL) << "unknown exception";
        return 1;
    }

    return 0;
}
