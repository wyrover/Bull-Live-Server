#include <iostream>
#include <MCoreApplication>

#include "mrtmpserver.hpp"
#include "BlsConf.hpp"
#include "mrtmpplayer.hpp"
#include "BlsMasterChannel.hpp"
#include "BlsChildChannel.hpp"
#include "BlsUtils.hpp"
#include "BlsStatistics.hpp"

#include <unistd.h>
#include <getopt.h>

#include "../../config.h"

static int childRun(MCoreApplication &app);
static void printVersion();
static void printHelp(int argc, char *argv[]);
static int getOpt(int argc ,char* argv[]);

static MString gs_confPath;

int main(int argc, char *argv[])
{
    if (getOpt(argc, argv) != E_SUCCESS) {
        return -1;
    }

    if (gs_confPath.empty()) {
        printf("you must specify conf file.\n");
        return -1;
    }

    MCoreApplication app(argc, argv);

    BlsConf::instance(gs_confPath);

    // master process
    vector<MRtmpServer *> rtmpServer;
    vector<BlsHostInfo> infos = BlsConf::instance()->getListenInfo();
    for (int i = 0; i < infos.size(); ++i) {
        BlsHostInfo &info = infos.at(i);
        MRtmpServer *s = new MRtmpServer;
        if (!s->listen(info.addr, info.port)) {
            log_error("listen %s:%d failed.", info.addr.c_str(), info.port);
            return -1;
        }

        rtmpServer.push_back(s);
    }

    int childProcessCount = BlsConf::instance()->getWorkerCount();
    int &internalListenPort = BlsConf::instance()->m_rtmpInternalPort;

    for (int i = 0; i < childProcessCount; ++i) {

        // open internal port
        MRtmpServer *s = new MRtmpServer;

        if (!s->listen("127.0.0.1", internalListenPort)) {
            log_error("listen local %d failed.", internalListenPort);
            return -1;
        }

        pid_t pid = fork();

        if (pid > 0) {
            s->close();

            // TODO check if we delete it ,app will crash.
            //s->deleteLater();
            ++internalListenPort;

            continue;
        } else if (pid == 0) {
            MString appName = MString().sprintf("%s:%d", "bls_worker", internalListenPort);
            app.setProcTitle(appName);

           return childRun(app);
        }
    }

    // crate master channle
    BlsMasterChannel masterChannel;
    if (!masterChannel.listen("127.0.0.1", 1940)) {
        log_error("BlsMasterChannel listen failed.");
        return -1;
    }

    BlsConf::instance()->m_processRole = Process_Role_Master;
    app.setProcTitle("bls_master");

    // close rtmp channel in master
    for (int i = 0; i < rtmpServer.size(); ++i) {
        MRtmpServer *s = rtmpServer.at(i);
        s->close();
        // TODO check if we delete it ,app will crash.
        //s->deleteLater();
    }

    int ret = app.exec();

    return ret;
}

int childRun(MCoreApplication &app)
{
    BlsChildChannel childChannel;
    if (childChannel.start() != E_SUCCESS) {
        return -1;
    }

    BlsConf::instance()->m_processRole = Process_Role_Child;

    return app.exec();
}

void printVersion()
{
    printf("%s\n", BLS_VERSION);
}

void printHelp(int argc, char *argv[])
{
    M_UNUSED(argc);

    char buf[2048];
    sprintf(buf, "Usage: %s [OPTION]... [FILE]...\n"
            "A Common Media Server for streaming rtmp, http flv live, HDS, HLS and so on.\n"
            "\n"
            "Mandatory arguments to long options are mandatory for short options too.\n"
            "  -f, --file           the configure file\n"
            "  -h, --help           display this help and exit\n"
            "  -v, --version        display version string and exit\n"
            "\n"
            "Exit status:\n"
            " 0     if OK,\n"
            " -1    if serious trouble\n"
            "\n"
            "Report %s bugs to 740936897@qq.com\n"
            ,argv[0]
            ,argv[0]);

    printf("%s", buf);
}

static struct option longOptions[] =
{
    {"file",        required_argument,  0, 'f'},
    {"help",        no_argument,        0, 'h'},
    {"version",     no_argument,        0, 'v'},
};

static const char* shortOptions = "f:hv";

int getOpt(int argc ,char* argv[])
{
    int c = -1;
    while ((c = getopt_long (argc, argv, shortOptions, longOptions, NULL)) != -1) {
        switch (c) {
        case 'f':
            if (optarg) {
                gs_confPath = optarg;
            }
            break;
        case 'v':
            printVersion();
            _exit(0);
        case 'h':
            printHelp(argc, argv);
            _exit(0);
        default:
            printf("see --help or -h\n");
            _exit(0);
        }
    }

    return E_SUCCESS;
}
