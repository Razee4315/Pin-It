#pragma once
//
// ElidedLabel — a single-line label that shortens its text with "…" to fit.
//
#include <QLabel>
#include <QResizeEvent>

// Elides its text to whatever width the layout gives it. A plain QLabel reports its full text width as its minimum, which made a
// long window title push the slider and unpin button out of the list.
class ElidedLabel : public QLabel
{
public:
    explicit ElidedLabel(const QString &text, QWidget *parent = nullptr)
        : QLabel(parent)
    {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        setFullText(text);
    }

    void setFullText(const QString &text)
    {
        m_fullText = text;
        updateElision();
    }

    QSize minimumSizeHint() const override
    {
        return QSize(0, QLabel::minimumSizeHint().height());
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QLabel::resizeEvent(event);
        updateElision();
    }

private:
    void updateElision()
    {
        setText(fontMetrics().elidedText(m_fullText, Qt::ElideRight, width()));
    }

    QString m_fullText;
};
