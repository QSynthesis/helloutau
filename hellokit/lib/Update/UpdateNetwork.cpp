#include "UpdateNetwork_p.h"

#include <QtCore/QCoreApplication>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkProxyFactory>

namespace hello::kit {

    namespace {

        constexpr int transferTimeout = 30000;

        // The proxy of the system for the requests of one manager. The global
        // QNetworkProxyFactory::setUseSystemConfiguration() would apply to every manager of the
        // application.
        class SystemProxyFactory : public QNetworkProxyFactory {
        public:
            QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query) override {
                return systemProxyForQuery(query);
            }
        };

    }

    QNetworkAccessManager *UpdateNetwork::createManager(QObject *parent) {
        auto manager = new QNetworkAccessManager(parent);
        manager->setProxyFactory(new SystemProxyFactory());
        manager->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
        manager->setTransferTimeout(transferTimeout);
        return manager;
    }

    QNetworkRequest UpdateNetwork::requestOf(const QUrl &url) {
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          QCoreApplication::applicationName() + QLatin1Char('/') +
                              QCoreApplication::applicationVersion());
        return request;
    }

    bool UpdateNetwork::isHttps(const QUrl &url) {
        return url.isValid() && url.scheme() == QLatin1String("https");
    }

}
