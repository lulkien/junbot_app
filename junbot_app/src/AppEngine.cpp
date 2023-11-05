#include "AppEngine.h"
#include "AppModel.h"
#include "Common.h"
#include "Constants_Def.h"
#include "Screen_Def.h"
#include "AppEnums.h"
#include "QMLHandler.h"
#include "QmlMQTTClient.h"
#include "QMQTTHandler.h"

AppEngine::AppEngine(QObject *parent)
    : QObject(parent)
{
    m_rootContext = m_engine.rootContext();
}

AppEngine::~AppEngine()
{
    // DELETE_DEFS;
    // DELETE_SCR_DEF;
}

void AppEngine::initEngine()
{
    connect(QML_HANDLER, &QMLHandler::notifyQMLEvent, this, &AppEngine::handleQmlEvent);
    qmlRegisterUncreatableType<AppEnums>("QmlCustomItem", 1, 0, "ENUMS", "Uncreatable");

    qmlRegisterType<QmlMqttClient>("MqttClient", 1, 0, "MqttClient");
    qRegisterMetaType<RobotNode>("RobotNode");
    qmlRegisterUncreatableType<QmlMqttSubscription>("MqttClient", 1, 0, "MqttSubscription", QLatin1String("Subscriptions are read-only"));

    // set context properties
    m_rootContext->setContextProperty("QmlHandler",     QML_HANDLER);
    m_rootContext->setContextProperty("CONST",          DEFS);
    m_rootContext->setContextProperty("SCREEN",         SCR_DEF);
    m_rootContext->setContextProperty("AppModel",       MODEL);
    m_rootContext->setContextProperty("QMqttHandler",   QMQTT_HANDLER);

    // init MQTT
    initMQTT();
}

void AppEngine::startApplication()
{
    m_engine.load(SCR_DEF->QML_APP());
}

void AppEngine::handleMqttConnected()
{
    m_handler->mqttSubcribe("robot1/login_userInforesponse");
}

void AppEngine::processMqttMessage(const QByteArray &message, const QString &topicName)
{
    LOG_DBG << "Process message:" << message << "in topic:" << topicName;
    if (topicName == "robot1/login_userInforesponse") {
        handleLoginResponse(message);
    }
}

void AppEngine::handleQmlEvent(int eventID)
{
    AppEnums::E_EVENT_t event = static_cast<AppEnums::E_EVENT_t>(eventID);
    LOG_INF << "Event:" << event;

    switch (event) {
    case AppEnums::E_EVENT_t::UserClickHome:
        MODEL->setCurrentScreenID(AppEnums::HomeScreen);
        break;
    case AppEnums::E_EVENT_t::UserClickControl:
        MODEL->setCurrentScreenID(AppEnums::ControlScreen);
        break;
    case AppEnums::E_EVENT_t::UserClickInfo:
        MODEL->setCurrentScreenID(AppEnums::UserScreen);
        break;
    case AppEnums::E_EVENT_t::LoginRequest:
        loginAuthenication();
        break;
    default:
        break;
    }
}

void AppEngine::initMQTT()
{
    m_handler = QMqttHandler::getInstance();
    m_handler->loadMQTTSetting(DEFS->DEVICE_PATH());
    m_handler->initBokerHost(DEFS->BROKER_PATH());

    connect(m_handler, &QMqttHandler::mqttMessageChanged, MODEL, &AppModel::setRobotMess);
    connect(m_handler, &QMqttHandler::mqttConnected, this, &AppEngine::handleMqttConnected);
    connect(m_handler, &QMqttHandler::mqttMessageReceived, this, &AppEngine::processMqttMessage);

    LOG_INF << "Hostname: " << MODEL->hostname();
    LOG_INF << "Port: " << MODEL->port();
    m_handler->connectMQTT(MODEL->hostname(), MODEL->port());
}

void AppEngine::initConnection()
{
    LOG_DBG;
    RobotNode node;
    node.ip = "xx.xx.xx.xx";
    node.name = "robot1";
    node.current_state_topic = "robot1/state";
    node.control_topic = "robot1/control";

    m_handler->RobotNodes.push_back(node);
    m_handler->mqttSubcribe(m_handler->RobotNodes.at(0));
    m_handler->mqttSubcribe("robot1/state");
}

void AppEngine::loginAuthenication()
{
    LOG_DBG << "USER: " << MODEL->userName();
    LOG_DBG << "PASS: " << MODEL->password();

    QJsonObject loginData;
    loginData.insert("username", MODEL->userName());
    loginData.insert("password", MODEL->password());
    m_handler->mqttPublish("robot1/login_request", loginData);
}

void AppEngine::handleLoginResponse(const QByteArray &message)
{
    QJsonDocument msgData = QJsonDocument::fromJson(message);
    QJsonObject obj = msgData.object();
    QString response = obj.value("response").toString();
    if (response == "success") {
        MODEL->setLoginStatus(true);
        initConnection();
    } else {
        MODEL->setLoginStatus(false);
        emit MODEL->notifyLoginFail();
    }
}


