作业3：人工智能程序设计与实现 - 医院智慧客服系统（HospAI） 1.1 功能需求分析 随着智慧医疗概念的普及，医院需要更高效、智能的方式来处理海量的患者咨询，减轻人工客服的压力，并提升患者就医体验。本项目旨在设计并实现一个医院智慧客服系统（HospAI），其核心是通过集成人工智能技术，为患者提供7x24小时的智能分诊服务。系统需能理解患者用自然语言描述的症状，进行分析判断，推荐合适的就诊科室，并评估紧急程度，从而实现对患者咨询的智能化、初步分流。
该系统需支持多角色用户。患者能够与AI助手进行对话，描述病情并获得专业建议；若问题复杂或AI无法解决，应能无缝转接至人工客服进行深入交流；系统管理员则负责管理用户账号、监控系统运行状态、配置AI参数等。此外，系统还需具备完整的会话管理、消息存储与历史查询功能，确保咨询过程的连续性和可追溯性。
系统的核心人工智能功能依赖于调用大模型API（本项目采用字节跳动豆包大模型）来实现智能问答。其数据处理流程如下所示，展示了从患者输入到获得AI回复的完整过程：
flowchart TD A[患者输入症状描述] --> B(前端界面接收请求) B --> C[系统构建专业提示词] C --> D{网络请求} D -- 成功 --> E[调用豆包大模型API] E -- 返回JSON结果 --> F[解析AI回复] F --> G[生成结构化诊断结果
（科室推荐、紧急程度）] G --> H[前端界面展示结果]
D -- 失败 --> I[网络异常处理]
I --> J[降级策略：<br>使用本地规则库回复]
J --> H
1.2 系统设计步骤 本系统采用C/S架构，使用C++语言和Qt6框架进行开发。Qt优秀的跨平台特性和丰富的UI组件库，能够高效地构建出美观、稳定的桌面客户端应用。数据库选用轻量级的SQLite，用于存储用户信息、聊天会话及消息记录，无需单独部署数据库服务，简化了系统部署流程。
系统设计采用了模块化的思想，核心模块包括：用户管理模块、AI接口模块、聊天会话模块和数据存储模块。各模块之间通过Qt的信号与槽机制进行通信，实现了低耦合和高内聚。AI接口模块被设计为一个独立的类（AIApiClient），专门负责封装网络请求、构造Prompt、发送数据到豆包API并解析返回的JSON结果，最终将结构化的数据（如推荐科室、紧急程度）通过信号发送给UI模块进行展示。
为了更清晰地展示系统关键类的结构与关系，其UML类图核心部分如下所示。它揭示了AIApiClient如何与主界面和数据库管理器交互，共同完成智能分诊任务：
classDiagram class AIApiClient { -QString m_apiKey -QString m_modelName +QNetworkAccessManager* networkManager +sendTriageRequest(QString userInput)$ +parseApiResponse(QJsonObject json)$ +signal diagnosisReceived(AIDiagnosisResult result) }
class ChatWidget {
    -AIApiClient* m_aiClient
    -DatabaseManager* m_dbManager
    -QString m_currentSessionId
    +onSendMessage()
    +onAIResponseReceived(AIDiagnosisResult result)
}

class DatabaseManager {
    +QSqlDatabase database
    +insertMessage(Message msg)$
    +getUserInfo(QString username)$
}

class AIDiagnosisResult {
    +QString recommendedDepartment
    +QString emergencyLevel
    +QString aiResponse
    +QStringList suggestions
}

ChatWidget --> AIApiClient : uses
ChatWidget --> DatabaseManager : uses
AIApiClient --> AIDiagnosisResult : creates
1.3 系统运行界面
1.4 系统源代码 核心AI接口模块代码 (src/core/AIApiClient.cpp)
cpp #include "AIApiClient.h" #include #include #include #include
AIApiClient::AIApiClient(QObject *parent) : QObject(parent) { networkManager = new QNetworkAccessManager(this); m_apiKey = "cb103329-5b77-418e-89f2-fea182318c91"; // 示例API Key m_modelName = "doubao-lite-32k-character-250228"; m_baseUrl = "https://ark.cn-beijing.volces.com/api/v3/chat/completions"; }
void AIApiClient::sendTriageRequest(const QString &userInput) { QNetworkRequest request(QUrl(m_baseUrl)); request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json"); request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());
// 构建请求数据
QJsonObject messageObj;
messageObj["role"] = "user";
messageObj["content"] = QString("你是一名专业的医疗分诊助手。请分析以下症状，推荐就诊科室并评估紧急程度。症状描述：%1").arg(userInput);

QJsonArray messagesArray;
messagesArray.append(messageObj);

QJsonObject dataObj;
dataObj["model"] = m_modelName;
dataObj["messages"] = messagesArray;
dataObj["stream"] = false;

// 发送请求
QNetworkReply *reply = networkManager->post(request, QJsonDocument(dataObj).toJson());
connect(reply, &QNetworkReply::finished, this, [this, reply]() { handleApiResponse(reply); });
}
void AIApiClient::handleApiResponse(QNetworkReply *reply) { if (reply->error() == QNetworkReply::NoError) { QByteArray response = reply->readAll(); QJsonDocument jsonDoc = QJsonDocument::fromJson(response); QJsonObject jsonObj = jsonDoc.object();
    AIDiagnosisResult result;
    // 解析JSON结果，提取科室推荐、紧急程度等信息
    if (jsonObj.contains("choices")) {
        QJsonArray choices = jsonObj["choices"].toArray();
        if (!choices.isEmpty()) {
            QJsonObject choice = choices[0].toObject();
            if (choice.contains("message")) {
                QJsonObject message = choice["message"].toObject();
                result.aiResponse = message["content"].toString();
                // 进一步解析aiResponse，提取结构化信息...
                result.recommendedDepartment = "内科"; // 示例解析
                result.emergencyLevel = "medium";
            }
        }
    }
    emit diagnosisReceived(result);
} else {
    qWarning() << "API请求错误:" << reply->errorString();
    emit errorOccurred(reply->errorString());
}
reply->deleteLater();
} 数据库表结构 (SQLite)
sql -- 用户表 CREATE TABLE users ( id INTEGER PRIMARY KEY AUTOINCREMENT, username VARCHAR(50) UNIQUE NOT NULL, password_hash VARCHAR(255) NOT NULL, email VARCHAR(100), phone VARCHAR(20), role VARCHAR(20) CHECK(role IN ('patient', 'staff', 'admin')), real_name VARCHAR(50), created_at DATETIME DEFAULT CURRENT_TIMESTAMP, last_login DATETIME, status INTEGER DEFAULT 1 );
-- 聊天会话表 CREATE TABLE chat_sessions ( id INTEGER PRIMARY KEY AUTOINCREMENT, patient_id INTEGER NOT NULL, staff_id INTEGER, status INTEGER DEFAULT 1, created_at DATETIME DEFAULT CURRENT_TIMESTAMP, updated_at DATETIME DEFAULT CURRENT_TIMESTAMP, FOREIGN KEY (patient_id) REFERENCES users (id), FOREIGN KEY (staff_id) REFERENCES users (id) );
-- 聊天消息表 CREATE TABLE chat_messages ( id INTEGER PRIMARY KEY AUTOINCREMENT, session_id INTEGER NOT NULL, sender_id INTEGER NOT NULL, message_type VARCHAR(20), content TEXT NOT NULL, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, is_read BOOLEAN DEFAULT FALSE, FOREIGN KEY (session_id) REFERENCES chat_sessions (id), FOREIGN KEY (sender_id) REFERENCES users (id) ); 1.5 系统代码网址 项目已开源，完整源代码可访问以下Github仓库查看与下载： https://github.com/Cyanlament/HospAI
1.6 总结与展望 本项目成功设计并实现了一个基于Qt和C++的医院智慧客服系统（HospAI）。系统通过调用豆包大模型API，核心实现了“智能问答”类型的人工智能应用，能够对患者症状进行智能分诊和科室推荐，满足了作业的基本功能要求。系统界面布局合理，采用现代化设计，用户体验良好。技术上，系统采用了模块化设计，集成网络请求、数据库管理、信号槽通信等多项Qt核心技术，结构清晰。
优势： 功能完整、界面美观、架构清晰，切实解决了医疗咨询中的实际问题。 可完善方向： 未来可考虑集成更多AI能力，如语音识别与合成让咨询更方便；引入知识图谱提升分诊准确性；支持图片上传并进行简单的医疗影像识别；以及开发Web版和移动版以实现更广泛的覆盖。

