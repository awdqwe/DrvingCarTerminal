#include "theorymanager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QCoreApplication>

TheoryManager::TheoryManager(QObject *parent) : QObject(parent) {
    t_currentIndex = 0;
    t_score = 0;
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

    t_questions = doc.array();
    t_currentIndex = 0;
    t_score = 0;
    t_answerRecords = QJsonArray();

    emit questionChanged();
    qDebug() << "成功加载题目数量:" << t_questions.size();
    return true;
}
// 返回当前题目文本
QString TheoryManager::currentQuestion() const {
    if (t_currentIndex < t_questions.size()) {
        return t_questions[t_currentIndex].toObject().value("question").toString();
    }
    return "答题结束";
}
// 返回当前选项列表 (QVariantList 方便 QML 直接用作 Model)
QVariantList TheoryManager::currentOptions() const {
    QVariantList options;
    if (t_currentIndex < t_questions.size()) {
        QJsonArray arr = t_questions[t_currentIndex].toObject().value("options").toArray();
        for (const QJsonValue &val : arr) {
            options << val.toString();
        }
    }
    return options;
}
// 提交答案并逻辑跳转
bool TheoryManager::submitAnswer(int optionIndex) {
    if (t_currentIndex >= t_questions.size()) return false;

    // 获取正确答案索引 (JSON 里的 answer 字段，通常设为 0-3)
    int correctAnswer = t_questions[t_currentIndex].toObject().value("answer").toInt();

    // 记录逻辑
    QJsonObject record;
    record["question"] = t_questions[t_currentIndex].toObject().value("question").toString();
    record["selected"] = optionIndex;
    record["correct"] = correctAnswer;
    record["isCorrect"] = (optionIndex == correctAnswer);

    t_answerRecords.append(record);

    if (optionIndex == correctAnswer) {
        t_score++;
    }

    // 移动到下一题
    t_currentIndex ++;

    if (t_currentIndex >= t_questions.size()) {
        emit quizFinished(t_score); // 触发结束信号
    } else {
        emit questionChanged(); // 触发 UI 更新
    }

    return (optionIndex == correctAnswer);
}
// 重置答题状态
void TheoryManager::reset() {
    t_currentIndex = 0;
    t_score = 0;
    t_answerRecords = QJsonArray();
    emit questionChanged();
}
// 生成答题结果 JSON
QJsonObject TheoryManager::getResult() const {
    QJsonObject obj;
    obj["type"] = "theory";
    obj["cardId"] = t_currentCard;
    obj["score"] = t_score;
    obj["total"] = (int)t_questions.size();
    return obj;
}
