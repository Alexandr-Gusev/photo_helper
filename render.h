#ifndef RENDER_H
#define RENDER_H

#include <QWidget>
#include <QPixmap>
#include <QAction>
#include <QContextMenuEvent>
#include <QRunnable>

class Render : public QWidget {
  Q_OBJECT

 public:
  explicit Render(QWidget* parent = 0);

  void setFolower(Render* folower);

  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

 private:
  static const QColor bg;
  static const int blocksSize = 8;

  QImage original;
  QPixmap blocks[blocksSize];
  QImage adjusted;
  QImage scaled;
  QPixmap pixmap;

  QActionGroup *modeGroup;
  QAction *adjustModeAct;
  QAction *cropModeAct;
  QAction *openAct;
  QAction *rotateAct;
  QAction *saveAct;

  Render *folower = 0;

  QString src;

  // percent
  double x = -1;
  double y = -1;
  double s = 0.01;

  int baseColor[3] = {-1, -1, -1};
  int avgColor[3] = {-1, -1, -1};

 private slots:
  void adjustMode();
  void cropMode();
  void open();
  void rotate();
  void save();

  void sChanged(double s);
  void baseColorChanged(QColor c);

 private:
  void calcAvgColor();
  void adjust();
  void adjustBlock(const int i);
  void scale();

  class AdjustTask : public QRunnable {
   public:
      Render* const render;
      const int i;
      AdjustTask(Render* const render, const int i) : render(render), i(i) {}
      void run() override {
        render->adjustBlock(i);
      }
  };

 signals:
  void sChangedSignal(double);
  void avgColorChangedSignal(QColor);
};

#endif // RENDER_H