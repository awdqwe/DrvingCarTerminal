#ifndef THEORYMANAGER_H
#define THEORYMANAGER_H

#include <QObject>
#include <QVariantList>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>

class TheoryManager: public QObject {
    Q_OBJECT
    // 将题目信息暴露给 QML 
    Q_PROPERTY(QString currentQuestion READ currentQuestion NOTIFY questionChanged)
    Q_PROPERTY(QVariantList currentOptions READ currentOptions NOTIFY questionChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY questionChanged)
    Q_PROPERTY(int totalQuestions READ totalQuestions NOTIFY questionChanged)

public:
    explicit TheoryManager(QObject *parent = nullptr);

    // 供 QML 调用
    // 1 加载题库 
    Q_INVOKABLE bool loadQuestions(const QString &filePath);
    // 2 提交并进入下一题
    Q_INVOKABLE bool submitAnswer(int optionIndex);
    // 3 重置答题状态
    Q_INVOKABLE void reset();
    // 4 导出结果
    Q_INVOKABLE QJsonObject getResult() const;
    // 5 设置当前卡号
    Q_INVOKABLE void setCurrentCard(const QString &cardId) { t_currentCard = cardId; }

    QString currentQuestion() const;
    QVariantList currentOptions() const;
    int currentIndex() const { return t_currentIndex; }
    int totalQuestions() const { return t_questions.size(); }
    

signals:
    void questionChanged(); // 切换题目
    void quizFinished(int score); // 答题结束

private:
    QJsonArray t_questions;
    int t_currentIndex = 0;
    int t_score = 0;
    // 答题记录缓存
    QJsonArray t_answerRecords;
    QString t_currentCard; // 缓存卡号
};

#endif // THEORYMANAGER_H
