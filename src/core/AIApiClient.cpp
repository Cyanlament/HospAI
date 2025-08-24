#include "AIApiClient.h"
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QSslError>
#include <QDebug>
#include <QApplication>

AIApiClient::AIApiClient(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
    , m_isConnected(false)
    , m_timeoutTimer(new QTimer(this))
    , m_currentRequestType(TriageRequest)
{
    // 设置默认配置
    setupDefaultConfig();
    
    // 配置超时定时器
    m_timeoutTimer->setSingleShot(true);
    m_timeoutTimer->setInterval(15000); // 15秒超时
    
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_currentReply) {
            m_currentReply->abort();
            emit apiError("请求超时，请检查网络连接");
        }
    });
    
    // SSL错误信号将在请求时连接
}

AIApiClient::~AIApiClient()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }
}

void AIApiClient::setupDefaultConfig()
{
    // 使用硬编码的API配置
    m_baseUrl = "https://ark.cn-beijing.volces.com/api/v3";
    m_apiKey = "cb103329-5b77-418e-89f2-fea182318c91";
    m_model = "doubao-lite-32k-character-250228";
    
    qDebug() << "AI API客户端初始化完成";
    qDebug() << "Base URL:" << m_baseUrl;
    qDebug() << "Model:" << m_model;
}

void AIApiClient::setApiConfig(const QString& baseUrl, const QString& apiKey, const QString& model)
{
    m_baseUrl = baseUrl;
    m_apiKey = apiKey;
    m_model = model;
    
    qDebug() << "API配置已更新 - URL:" << baseUrl << "Model:" << model;
}

bool AIApiClient::isConnected() const
{
    return m_isConnected;
}

QString AIApiClient::getLastError() const
{
    return m_lastError;
}

void AIApiClient::sendTriageRequest(const QString& userInput, const QString& conversationHistory)
{
    if (m_currentReply) {
        qDebug() << "请求正在进行中，忽略新请求";
        return;
    }
    
    m_currentRequestType = TriageRequest;
    emit requestStarted();
    
    QString systemPrompt = createTriagePrompt(userInput, conversationHistory);
    QJsonObject requestBody = createRequestBody(systemPrompt, userInput);
    
    QNetworkRequest request = createApiRequest();
    
    QJsonDocument doc(requestBody);
    m_currentReply = m_networkManager->post(request, doc.toJson());
    
    connect(m_currentReply, &QNetworkReply::finished, 
            this, &AIApiClient::handleTriageResponse);
    connect(m_currentReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &AIApiClient::handleNetworkError);
    connect(m_currentReply, &QNetworkReply::sslErrors,
            this, &AIApiClient::handleSslErrors);
    
    m_timeoutTimer->start();
    
    qDebug() << "发送智能分诊请求:" << userInput;
}

void AIApiClient::sendSymptomAnalysis(const QString& symptoms, int age, const QString& gender)
{
    if (m_currentReply) {
        qDebug() << "请求正在进行中，忽略新请求";
        return;
    }
    
    m_currentRequestType = SymptomRequest;
    emit requestStarted();
    
    QString systemPrompt = createSymptomPrompt(symptoms, age, gender);
    QJsonObject requestBody = createRequestBody(systemPrompt, symptoms);
    
    QNetworkRequest request = createApiRequest();
    
    QJsonDocument doc(requestBody);
    m_currentReply = m_networkManager->post(request, doc.toJson());
    
    connect(m_currentReply, &QNetworkReply::finished, 
            this, &AIApiClient::handleSymptomResponse);
    connect(m_currentReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &AIApiClient::handleNetworkError);
    connect(m_currentReply, &QNetworkReply::sslErrors,
            this, &AIApiClient::handleSslErrors);
    
    m_timeoutTimer->start();
    
    qDebug() << "发送症状分析请求:" << symptoms;
}

void AIApiClient::sendDepartmentRecommendation(const QString& symptoms, const QString& analysis)
{
    if (m_currentReply) {
        qDebug() << "请求正在进行中，忽略新请求";
        return;
    }
    
    m_currentRequestType = DepartmentRequest;
    emit requestStarted();
    
    QString systemPrompt = createDepartmentPrompt(symptoms, analysis);
    QJsonObject requestBody = createRequestBody(systemPrompt, symptoms);
    
    QNetworkRequest request = createApiRequest();
    
    QJsonDocument doc(requestBody);
    m_currentReply = m_networkManager->post(request, doc.toJson());
    
    connect(m_currentReply, &QNetworkReply::finished, 
            this, &AIApiClient::handleDepartmentResponse);
    connect(m_currentReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &AIApiClient::handleNetworkError);
    connect(m_currentReply, &QNetworkReply::sslErrors,
            this, &AIApiClient::handleSslErrors);
    
    m_timeoutTimer->start();
    
    qDebug() << "发送科室推荐请求:" << symptoms;
}

QNetworkRequest AIApiClient::createApiRequest()
{
    QNetworkRequest request(QUrl(m_baseUrl + "/chat/completions"));
    
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());
    request.setRawHeader("User-Agent", "");
    
    return request;
}

QJsonObject AIApiClient::createRequestBody(const QString& systemPrompt, const QString& userMessage)
{
    QJsonObject requestBody;
    requestBody["model"] = m_model;
    requestBody["stream"] = false;
    
    QJsonArray messages;
    
    // 系统消息
    QJsonObject systemMsg;
    systemMsg["role"] = "system";
    systemMsg["content"] = systemPrompt;
    messages.append(systemMsg);
    
    // 用户消息
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userMessage;
    messages.append(userMsg);
    
    requestBody["messages"] = messages;
    
    // 参数设置
    requestBody["temperature"] = 0.7;
    requestBody["max_tokens"] = 1000;
    requestBody["top_p"] = 0.9;
    
    return requestBody;
}

QString AIApiClient::createTriagePrompt(const QString& userInput, const QString& history)
{
    QString prompt = R"(
你是一个专业的医院智能分诊助手，具有丰富的医学知识和临床经验。请根据患者的症状描述进行智能分诊分析。

## 你的职责：
1. 分析患者症状，评估紧急程度
2. 推荐最合适的科室
3. 提供初步的建议和注意事项
4. 判断是否需要紧急就医
5. 提供温馨的就医指导

## 分析要求：
- 紧急程度分级：critical(危急)、high(紧急)、medium(一般)、low(轻微)
- 科室推荐要准确具体
- 语言要温暖亲切，易于理解
- 必要时建议转人工客服

## 医院科室参考：
内科、外科、妇科、儿科、皮肤科、眼科、耳鼻喉科、口腔科、骨科、神经内科、心内科、消化内科、呼吸内科、内分泌科、泌尿外科、胸外科、神经外科、整形外科、急诊科

## 回复格式要求：
请用温馨、专业的语调回复，包含：
1. 症状确认和理解
2. 初步分析
3. 科室推荐及理由  
4. 注意事项
5. 后续建议

请注意：你不能进行确诊，只能提供分诊建议。严重症状务必建议立即就医。
)";

    if (!history.isEmpty()) {
        prompt += "\n\n## 对话历史：\n" + history;
    }

    return prompt;
}

QString AIApiClient::createSymptomPrompt(const QString& symptoms, int age, const QString& gender)
{
    QString prompt = R"(
你是一个专业的症状分析专家，请对患者描述的症状进行详细分析。

## 分析维度：
1. 症状特征分析
2. 可能的病因
3. 严重程度评估
4. 需要关注的并发症状
5. 就医紧急性判断

## 患者信息：
症状描述：%1
)";

    if (age > 0) {
        prompt += QString("年龄：%1岁\n").arg(age);
    }
    if (!gender.isEmpty()) {
        prompt += QString("性别：%1\n").arg(gender);
    }

    prompt += R"(
请提供专业、详细的症状分析，但避免确诊，重点关注分诊指导。
)";

    return prompt.arg(symptoms);
}

QString AIApiClient::createDepartmentPrompt(const QString& symptoms, const QString& analysis)
{
    QString prompt = R"(
你是一个科室推荐专家，请根据患者症状和分析结果，推荐最合适的科室。

## 患者症状：
%1

## 初步分析：
%2

## 请重点提供：
1. 最推荐的科室及详细理由
2. 备选科室方案
3. 就诊优先级
4. 挂号建议（普通/专家门诊）
5. 就诊前准备事项

请给出清晰、实用的科室推荐建议。
)";

    return prompt.arg(symptoms, analysis);
}

void AIApiClient::handleTriageResponse()
{
    m_timeoutTimer->stop();
    
    if (!m_currentReply) {
        return;
    }
    
    QNetworkReply* reply = m_currentReply;
    m_currentReply = nullptr;
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        
        qDebug() << "收到分诊响应:" << doc.toJson(QJsonDocument::Compact);
        
        AIDiagnosisResult result = parseApiResponse(doc);
        m_isConnected = true;
        emit connectionStatusChanged(true);
        emit triageResponseReceived(result);
        
    } else {
        QString error = QString("网络请求失败: %1").arg(reply->errorString());
        m_lastError = error;
        m_isConnected = false;
        emit connectionStatusChanged(false);
        emit apiError(error);
    }
    
    reply->deleteLater();
    emit requestFinished();
}

void AIApiClient::handleSymptomResponse()
{
    m_timeoutTimer->stop();
    
    if (!m_currentReply) {
        return;
    }
    
    QNetworkReply* reply = m_currentReply;
    m_currentReply = nullptr;
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        
        AIDiagnosisResult result = parseApiResponse(doc);
        m_isConnected = true;
        emit connectionStatusChanged(true);
        emit symptomAnalysisReceived(result);
        
    } else {
        QString error = QString("症状分析请求失败: %1").arg(reply->errorString());
        m_lastError = error;
        emit apiError(error);
    }
    
    reply->deleteLater();
    emit requestFinished();
}

void AIApiClient::handleDepartmentResponse()
{
    m_timeoutTimer->stop();
    
    if (!m_currentReply) {
        return;
    }
    
    QNetworkReply* reply = m_currentReply;
    m_currentReply = nullptr;
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        
        AIDiagnosisResult result = parseApiResponse(doc);
        m_isConnected = true;
        emit connectionStatusChanged(true);
        emit departmentRecommendationReceived(result);
        
    } else {
        QString error = QString("科室推荐请求失败: %1").arg(reply->errorString());
        m_lastError = error;
        emit apiError(error);
    }
    
    reply->deleteLater();
    emit requestFinished();
}

AIDiagnosisResult AIApiClient::parseApiResponse(const QJsonDocument& response)
{
    AIDiagnosisResult result;
    
    QJsonObject rootObj = response.object();
    
    if (rootObj.contains("choices") && rootObj["choices"].isArray()) {
        QJsonArray choices = rootObj["choices"].toArray();
        if (!choices.isEmpty()) {
            QJsonObject firstChoice = choices[0].toObject();
            if (firstChoice.contains("message")) {
                QJsonObject message = firstChoice["message"].toObject();
                result.aiResponse = message["content"].toString();
                
                // 解析AI回复内容，提取结构化信息
                parseAIResponseContent(result);
            }
        }
    }
    
    if (result.aiResponse.isEmpty()) {
        result.aiResponse = "抱歉，暂时无法获取AI回复，请稍后重试或转人工客服。";
        result.needsHumanConsult = true;
    }
    
    return result;
}

void AIApiClient::parseAIResponseContent(AIDiagnosisResult& result)
{
    QString content = result.aiResponse.toLower();
    
    // 判断紧急程度
    if (content.contains("危急") || content.contains("critical") || content.contains("立即就医")) {
        result.emergencyLevel = "critical";
    } else if (content.contains("紧急") || content.contains("high") || content.contains("尽快就医")) {
        result.emergencyLevel = "high";
    } else if (content.contains("一般") || content.contains("medium")) {
        result.emergencyLevel = "medium";
    } else {
        result.emergencyLevel = "low";
    }
    
    // 提取科室推荐
    QStringList departments = {"内科", "外科", "妇科", "儿科", "皮肤科", "眼科", "耳鼻喉科", 
                              "口腔科", "骨科", "神经内科", "心内科", "消化内科", "呼吸内科", 
                              "内分泌科", "泌尿外科", "胸外科", "神经外科", "整形外科", "急诊科"};
    
    for (const QString& dept : departments) {
        if (content.contains(dept)) {
            result.recommendedDepartment = dept;
            break;
        }
    }
    
    // 判断是否需要人工咨询
    result.needsHumanConsult = content.contains("转人工") || content.contains("人工客服") || 
                              content.contains("专业医生") || result.emergencyLevel == "critical";
}

void AIApiClient::handleNetworkError(QNetworkReply::NetworkError error)
{
    m_timeoutTimer->stop();
    
    QString errorMsg;
    switch (error) {
        case QNetworkReply::ConnectionRefusedError:
            errorMsg = "连接被拒绝，请检查网络设置";
            break;
        case QNetworkReply::RemoteHostClosedError:
            errorMsg = "远程主机关闭连接";
            break;
        case QNetworkReply::HostNotFoundError:
            errorMsg = "无法找到服务器，请检查网络连接";
            break;
        case QNetworkReply::TimeoutError:
            errorMsg = "请求超时，请稍后重试";
            break;
        case QNetworkReply::SslHandshakeFailedError:
            errorMsg = "SSL连接失败";
            break;
        default:
            errorMsg = QString("网络错误: %1").arg(m_currentReply ? m_currentReply->errorString() : "未知错误");
            break;
    }
    
    m_lastError = errorMsg;
    m_isConnected = false;
    emit connectionStatusChanged(false);
    emit apiError(errorMsg);
    
    if (m_currentReply) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    
    emit requestFinished();
}

void AIApiClient::handleSslErrors(const QList<QSslError>& errors)
{
    qDebug() << "SSL错误数量:" << errors.size();
    for (const QSslError& error : errors) {
        qDebug() << "SSL错误:" << error.errorString();
    }
    
    // 在生产环境中，应该更严格地处理SSL错误
    // 这里为了测试方便，忽略SSL错误
    if (m_currentReply) {
        m_currentReply->ignoreSslErrors();
    }
} 
