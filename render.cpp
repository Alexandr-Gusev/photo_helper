#include "render.h"

#include <QPainter>
#include <QFileDialog>
#include <QMenu>
#include <QThreadPool>
#include <QMessageBox>
#include <QDateTime>

const QColor Render::bg = QColor(34, 89, 125);

Render::Render(QWidget *parent) : QWidget(parent) {
  adjustModeAct = new QAction(tr("&Adjust color"), this);
  adjustModeAct->setCheckable(true);
  connect(adjustModeAct, &QAction::triggered, this, &Render::adjustMode);

  cropModeAct = new QAction(tr("&Crop image"), this);
  cropModeAct->setCheckable(true);
  connect(cropModeAct, &QAction::triggered, this, &Render::cropMode);

  modeGroup = new QActionGroup(this);
  modeGroup->addAction(adjustModeAct);
  modeGroup->addAction(cropModeAct);
  adjustModeAct->setChecked(true);

  openAct = new QAction(tr("&Open"), this);
  connect(openAct, &QAction::triggered, this, &Render::open);

  rotateAct = new QAction(tr("&Rotate"), this);
  connect(rotateAct, &QAction::triggered, this, &Render::rotate);

  saveAct = new QAction(tr("&Save"), this);
  connect(saveAct, &QAction::triggered, this, &Render::save);

  original = QImage(size().width(), size().height(), QImage::Format_RGB32);
  QPainter painter(&original);
  painter.fillRect(original.rect(), bg);

  adjust();
}

void Render::setFolower(Render* folower) {
  this->folower = folower;

  connect(this, SIGNAL(sChangedSignal(double)), folower, SLOT(sChanged(double)), Qt::QueuedConnection);
  connect(folower, SIGNAL(sChangedSignal(double)), this, SLOT(sChanged(double)), Qt::QueuedConnection);

  connect(this, SIGNAL(avgColorChangedSignal(QColor)), folower, SLOT(baseColorChanged(QColor)), Qt::QueuedConnection);

  connect(adjustModeAct, &QAction::triggered, folower, &Render::adjustMode);
  connect(cropModeAct, &QAction::triggered, folower, &Render::cropMode);

  connect(folower->adjustModeAct, &QAction::triggered, this, &Render::adjustMode);
  connect(folower->cropModeAct, &QAction::triggered, this, &Render::cropMode);
}

void Render::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.drawPixmap(0, 0, pixmap);
  if (src == "") {
    painter.setPen(Qt::yellow);
    painter.drawText(10, 10, tr("Follow popup menu and use left mouse button"));
  }
  if (x >= 0) {
    int w = s * size().width();
    int h = s * size().height();
    int l = x * size().width() - w / 2;
    int t = y * size().height() - h / 2;

    painter.setPen(Qt::yellow);
    painter.drawRect(l, t, w, h);

    if (adjustModeAct->isChecked()) {
      painter.drawText(
        l + w + 10, t + h + 10,
        tr("%1 / %2 / %3").arg(avgColor[0]).arg(avgColor[1]).arg(avgColor[2]));
    } else {
      painter.drawText(
        l + w + 10, t + h + 10,
        tr("%1 x %2").arg(s * original.size().width()).arg(s * original.size().height()));
    }
  }
}

void Render::resizeEvent(QResizeEvent*) {
  scale();
}

void Render::contextMenuEvent(QContextMenuEvent* event) {
  QMenu menu(this);
  menu.addAction(adjustModeAct);
  menu.addAction(cropModeAct);
  menu.addSeparator();
  menu.addAction(openAct);
  menu.addAction(rotateAct);
  menu.addSeparator();
  menu.addAction(saveAct);
  menu.exec(event->globalPos());
}

void Render::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    x = double(event->pos().x()) / size().width();
    y = double(event->pos().y()) / size().height();
    if (adjustModeAct->isChecked()) {
      calcAvgColor();
      adjust();
    }
    repaint();
  }
}

void Render::wheelEvent(QWheelEvent* event) {
  QPoint degrees = event->angleDelta() / 8;
  QPoint steps = degrees / 15;
  s += 0.01 * steps.y();
  if (s < 0.01) s = 0.01;
  if (s > 100) s = 100;
  if (x >= 0) {
    if (adjustModeAct->isChecked()) {
      calcAvgColor();
      adjust();
    }
    repaint();
  }
  emit sChangedSignal(s);
}

void Render::adjustMode() {
  adjustModeAct->setChecked(true);
  if (x >= 0) {
    calcAvgColor();
    adjust();
    repaint();
  }
}

void Render::cropMode() {
  cropModeAct->setChecked(true);

  for (int i = 0; i < 3; i++) {
    avgColor[i] = -1;
  }

  adjust();
  repaint();
}

void Render::open() {
  QFileDialog dialog(this);
  dialog.setFileMode(QFileDialog::ExistingFile);
  dialog.setNameFilter(tr("Images (*.bmp *.png *.jpg *.jpeg)"));
  if (dialog.exec()) {
    src = dialog.selectedFiles()[0];
    original.load(src);

    x = -1;
    y = -1;

    for (int i = 0; i < 3; i++) {
      avgColor[i] = -1;
    }

    adjust();
    repaint();
  }
}

void Render::rotate() {
  x = -1;
  y = -1;

  for (int i = 0; i < 3; i++) {
    avgColor[i] = -1;
  }

  QPoint center = original.rect().center();
  QMatrix matrix;
  matrix.translate(center.x(), center.y());
  matrix.rotate(90);
  original = original.transformed(matrix);
  adjust();
  repaint();
}

void Render::save() {
  QFileInfo fi(src);
  QString dst = fi.dir().filePath(
    fi.baseName() +
    " " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh-mm-ss") + ".jpg");
  bool res;
  if (adjustModeAct->isChecked()) {
    res = adjusted.save(dst, nullptr, 80);
  } else {
    int w = s * original.size().width();
    int h = s * original.size().height();
    int l = x * original.size().width() - w / 2;
    int t = y * original.size().height() - h / 2;
    QImage part(w, h, QImage::Format_RGB32);
    QPainter painter(&part);
    painter.drawPixmap(0, 0, w, h, QPixmap::fromImage(original), l, t, w, h);
    res = part.save(dst, nullptr, 80);
  }
  if (!res) {
    QMessageBox msg(this);
    msg.setIcon(QMessageBox::Information);
    msg.setText(tr("Unknown error"));
    msg.setWindowTitle(tr("Message"));
    msg.setStandardButtons(QMessageBox::Ok);
    msg.exec();
  }
}

void Render::sChanged(double s) {
  this->s = s;
  if (x >= 0) {
    if (adjustModeAct->isChecked()) {
      calcAvgColor();
      adjust();
    }
    repaint();
  }
}

void Render::baseColorChanged(QColor c) {
  c.getRgb(&baseColor[0], &baseColor[1], &baseColor[2]);
  adjust();
  repaint();
}

void Render::calcAvgColor() {
  for (int i = 0; i < 3; i++) {
    avgColor[i] = 0;
  }

  int w = s * original.size().width();
  int h = s * original.size().height();

  int l = x * original.size().width() - w / 2;
  int t = y * original.size().height() - h / 2;
  int r = l + w;
  int b = t + h;

  if (l < 0) l = 0;
  if (t < 0) t = 0;

  if (r > original.size().width()) r = original.size().width();
  if (b > original.size().height()) b = original.size().height();

  if (r == l) r++;
  if (b == t) b++;

  for (int x = l; x < r; x++) {
    for (int y = t; y < b; y++) {
      QRgb c = original.pixel(x, y);
      avgColor[0] += qRed(c);
      avgColor[1] += qGreen(c);
      avgColor[2] += qBlue(c);
    }
  }

  int n = (r - l) * (b - t);
  for (int i = 0; i < 3; i++) {
    avgColor[i] /= n;
    if (!avgColor[i]) {
      avgColor[i] = 1;
    }
    if (avgColor[i] > 255) {
      avgColor[i] = 255;
    }
  }

  if (folower) {
    emit avgColorChangedSignal(QColor(avgColor[0], avgColor[1], avgColor[2]));
  }
}

void Render::adjust() {
  if (baseColor[0] < 0 || avgColor[0] < 0) {
    adjusted = original;
  } else {
    adjusted = QImage(original.size().width(), original.size().height(), QImage::Format_RGB32);

    for (int i = 0; i < blocksSize; i++) {
      QThreadPool::globalInstance()->start(new AdjustTask(this, i));
    }
    QThreadPool::globalInstance()->waitForDone();

    int w = original.size().width() / blocksSize;

    QPainter painter(&adjusted);
    for (int i = 0; i < blocksSize; i++) {
      painter.drawPixmap(i * w, 0, blocks[i]);
    }
  }
  scale();
}

void Render::adjustBlock(const int i) {
  double k[3];
  for (int i = 0; i < 3; i++) {
    k[i] = double(baseColor[i]) / avgColor[i];
  }

  int w = original.size().width() / blocksSize;
  int l = i * w;
  int r = l + w;
  if (r > original.size().width() || i == blocksSize - 1) r = original.size().width();

  blocks[i] = QPixmap(r - l, original.size().height());
  QPainter painter(&blocks[i]);
  for (int x = l; x < r; x++) {
    for (int y = 0; y < original.size().height(); y++) {
      QRgb c = original.pixel(x, y);

      int r = k[0] * qRed(c);
      int g = k[1] * qGreen(c);
      int b = k[2] * qBlue(c);

      if (r < 0) r = 0;
      if (r > 255) r = 255;

      if (g < 0) g = 0;
      if (g > 255) g = 255;

      if (b < 0) b = 0;
      if (b > 255) b = 255;

      painter.setPen(QColor(r, g, b));
      painter.drawPoint(x - l, y);
    }
  }
}

void Render::scale() {
  scaled = adjusted.scaled(size().width(), size().height());
  pixmap = QPixmap::fromImage(scaled);
}
