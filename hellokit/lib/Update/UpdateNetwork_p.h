#ifndef HELLOKIT_UPDATE_UPDATENETWORK_P_H
#define HELLOKIT_UPDATE_UPDATENETWORK_P_H

#include <QtCore/QUrl>
#include <QtNetwork/QNetworkRequest>

class QNetworkAccessManager;
class QObject;

namespace hello::kit {

    /// The network access shared by the requests of the library.
    class UpdateNetwork {
    public:
        /// Returns a new manager owned by \a parent, which uses the proxy of the system, fails a
        /// redirect from HTTPS to HTTP, and fails a request that receives no data for 30 seconds.
        static QNetworkAccessManager *createManager(QObject *parent);

        /// Returns a request for \a url with the user agent of the application, which GitHub
        /// requires.
        static QNetworkRequest requestOf(const QUrl &url);

        /// Returns whether \a url is a valid HTTPS URL.
        static bool isHttps(const QUrl &url);
    };

}

#endif // HELLOKIT_UPDATE_UPDATENETWORK_P_H
