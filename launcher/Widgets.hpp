#ifndef LAUNCHER_WIDGETS_HPP
#define LAUNCHER_WIDGETS_HPP

#include <QAbstractButton>
#include <QPushButton>
#include <QPixmap>
#include <QWidget>

QString DisplayFamily();
QFont DisplayFont(int pointSize, int weight);

class IconButton : public QPushButton
{
    Q_OBJECT

public:
    IconButton(const QIcon& icon, const QString& text, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};

class Backdrop : public QWidget
{
    Q_OBJECT

public:
    explicit Backdrop(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPixmap mGrain;
    QPixmap mMark;
};

class NavItem : public QAbstractButton
{
    Q_OBJECT

public:
    explicit NavItem(const QString& text, QWidget* parent = nullptr);
    QSize sizeHint() const override;

    void SetBadge(const QString& badge);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QString mBadge;
    bool mHover = false;
};

class ProfileChip : public QAbstractButton
{
    Q_OBJECT

public:
    explicit ProfileChip(QWidget* parent = nullptr);
    QSize sizeHint() const override;

    void SetProfile(const QPixmap& face, const QString& username, const QString& profileName);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QPixmap mFace;
    QString mUsername;
    QString mProfileName;
    bool mHover = false;
};

class PlayButton : public QAbstractButton
{
    Q_OBJECT

public:
    explicit PlayButton(QWidget* parent = nullptr);
    QSize sizeHint() const override;

    void SetLabels(const QString& title, const QString& detail, bool attention);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QString mDetail;
    bool mAttention = false;
    bool mHover = false;
};

#endif
