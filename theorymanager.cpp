#include "theorymanager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QCoreApplication>

TheoryManager::TheoryManager(QObject *parent) : QObject(parent) {
    m_currentIndex = 0;
    m_score = 0;
}

// 加载外部 JSON 题库
bool TheoryManager::loadQuestions(const QString &fileName) {
    // 默认在程序运行同级目录下寻找文件
    QString filePath = QCoreApplication::applicationDirPath() + "/" + fileName;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "无法打开题库文件:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isArray()) {
        qDebug() << "题库格式错误，应为 JSON 数组";
        return false;
    }

    m_questions = doc.array();
    m_currentIndex = 0;
    m_score = 0;

    emit questionChanged();
    qDebug() << "成功加载题目数量:" << m_questions.size();
    return true;
}

// 返回当前题目文本
QString TheoryManager::currentQuestion() const {
    if (m_currentIndex < m_questions.size()) {
        return m_questions[m_currentIndex].toObject().value("question").toString();
    }
    return "答题结束";
}

// 返回当前选项列表 (QVariantList 方便 QML 直接用作 Model)
QVariantList TheoryManager::currentOptions() const {
    QVariantList options;
    if (m_currentIndex < m_questions.size()) {
        QJsonArray arr = m_questions[m_currentIndex].toObject().value("options").toArray();
        for (const QJsonValue &val : arr) {
            options << val.toString();
        }
    }
    return options;
}

// 提交答案并逻辑跳转
bool TheoryManager::submitAnswer(int optionIndex) {
    if (m_currentIndex >= m_questions.size()) return false;

    // 获取正确答案索引 (JSON 里的 answer 字段，通常设为 0-3)
    int correctAnswer = m_questions[m_currentIndex].toObject().value("answer").toInt();

    if (optionIndex == correctAnswer) {
        m_score++;
    }

    // 移动到下一题
    m_currentIndex++;

    if (m_currentIndex >= m_questions.size()) {
        emit quizFinished(m_score); // 触发结束信号
    } else {
        emit questionChanged(); // 触发 UI 更新
    }

    return (optionIndex == correctAnswer);
}

void TheoryManager::reset() {
    m_currentIndex = 0;
    m_score = 0;
    emit questionChanged();
}