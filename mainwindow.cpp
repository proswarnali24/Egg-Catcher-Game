#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPainter>
#include <QRandomGenerator>
#include <QtMath>
#include <algorithm>
#include <QDebug>
// QPainterPath is not needed as we are using the original basket shape and low-level egg drawing

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
    moveLeft(false), moveRight(false)
{
    ui->setupUi(this);
    setFocusPolicy(Qt::StrongFocus);


    grid_box = 20;
    grid_size = ui->frame->frameSize().width();
    cols = grid_size / grid_box;
    rows = grid_size / grid_box;

    QPixmap bg(grid_size, grid_size);
    bg.fill(Qt::black);
    QPainter g(&bg);
    g.setPen(QPen(Qt::darkGray, 1));
    for (int i = 0; i <= cols; i++)
        g.drawLine(i * grid_box, 0, i * grid_box, grid_size);
    for (int j = 0; j <= rows; j++)
        g.drawLine(0, j * grid_box, grid_size, j * grid_box);
    g.end();
    background = bg;
    ui->frame->setPixmap(background);

    // --- Game setup ---
    score = 0;
    lives = 5; // Total five lives
    basket = QPointF(cols / 2.0f, rows - 3);

    basketXVelocity = 0.0f;
    basketTargetVel = 0.0f;
    basketAccel = 20.0f;
    basketMaxVel = 12.0f;

    // -- Load sound files
    soundCatch.setSource(QUrl::fromLocalFile("/Users/pavel/Documents/Graphics/EggCatcher/EggCatcher/sfx/catch.wav"));
    soundCatch.setVolume(0.8f);   // 0.0 – 1.0

    soundLose.setSource(QUrl::fromLocalFile("/Users/pavel/Documents/Graphics/EggCatcher/EggCatcher/sfx/lose.wav"));
    soundLose.setVolume(0.9f);



    qDebug() << "Catch status:" << soundCatch.status();
    qDebug() << "Lose status:" << soundLose.status();

    // --- Timer setup ---
    gameTimer = new QTimer(this);
    connect(gameTimer, &QTimer::timeout, this, &MainWindow::gameTick);
    gameTimer->start(16);  // ~60 FPS
    frameClock.start();
    fixedDelta = 1.0f / 120.0f;
    accumulator = 0.0f;
    gameOver = false;

    // Select 4 predefined drop columns
    dropColumns.clear();
    int mid = cols / 2;
    dropColumns = { mid - 10, mid - 5, mid + 5, mid + 10};
    std::sort(dropColumns.begin(), dropColumns.end());

    columnTimers.resize(dropColumns.size());
    columnDelays.resize(dropColumns.size());
    for (int i = 0; i < dropColumns.size(); ++i) {
        columnTimers[i] = 0.0f;
        columnDelays[i] = 6.0f + QRandomGenerator::global()->bounded(2.0f);
    }

    currentColumnIndex = 0;
    globalSpawnTimer = 0.0f;
    spawnInterval = 1.0f;
    globalTime = 0.f;

    lastEdgeSpawnTime = -100.0f; // initialize far in past
    edgeSpawnCooldown = 4.0f;    // base 4 sec gap between opposite edge spawns
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (event->isAutoRepeat()) return;

    if (gameOver && event->key() == Qt::Key_R) {
        resetGame();
        return;
    }

    if (!gameRunning && event->key() == Qt::Key_Return) {
        gameRunning = true;
        return;
    }

    if (gameOver) return;
    if (!gameRunning) return;

    // Using Arrow Keys
    if (event->key() == Qt::Key_Left)
        moveLeft = true;
    else if (event->key() == Qt::Key_Right)
        moveRight = true;
}

void MainWindow::keyReleaseEvent(QKeyEvent *event) {
    if (event->isAutoRepeat()) return;
    if (!gameRunning || gameOver) return;
    if (gameOver) return;

    // Using Arrow Keys
    if (event->key() == Qt::Key_Left)
        moveLeft = false;
    else if (event->key() == Qt::Key_Right)
        moveRight = false;
}

void MainWindow::resetGame() {
    score = 0;
    lives = 5; // Reset to 5 lives
    eggs.clear();
    basket = QPointF(cols / 2.0f, rows - 3);
    basketXVelocity = 0.0f;
    basketTargetVel = 0.0f;
    gameOver = false;
    accumulator = 0.0f;
    frameClock.restart();
    gameRunning = true;
    gameTimer->start();
}


void MainWindow::gameTick() {
    if (gameOver) {
        drawGameOver();
        return;
    }

    // If game hasn't started yet → just draw start screen
    if (!gameRunning) {
        drawStartScreen();
        return;
    }

    float dt = frameClock.restart() / 1000.0f;
    accumulator += dt;

    while (accumulator >= fixedDelta) {
        updatePhysics(fixedDelta);
        accumulator -= fixedDelta;
    }

    float alpha = accumulator / fixedDelta;
    drawGame(alpha);

    // --- Check game over ---
    if (lives <= 0) {
        gameOver = true;
        drawGameOver();
        gameTimer->stop();
    }
}

void MainWindow::drawStartScreen() {
    QPixmap pix = background;
    QPainter p(&pix);

    p.setPen(Qt::white);
    p.setFont(QFont("Arial", 20, QFont::Bold));
    p.drawText(pix.rect(), Qt::AlignCenter,
               "EGG CATCHER\n\nPress ENTER to Start");

    p.end();
    ui->frame->setPixmap(pix);
}


void MainWindow::drawGameOver() {
    QPixmap pix = background;
    QPainter p(&pix);
    p.setPen(Qt::red);
    p.setFont(QFont("Arial", 24, QFont::Bold));
    p.drawText(pix.rect(), Qt::AlignCenter, "GAME OVER\nPress R to Restart");
    p.end();
    ui->frame->setPixmap(pix);
}

void MainWindow::updatePhysics(float dt) {
    if (gameOver) return;

    globalTime += dt;
    globalSpawnTimer += dt;

    if (globalSpawnTimer >= spawnInterval) {
        int col = dropColumns[currentColumnIndex];
        bool isEdgeCol = (col == dropColumns.front() || col == dropColumns.back());
        bool canSpawn = true;

        // --- Dynamic edge cooldown scaling with score ---
        float dynamicEdgeCooldown = std::max(1.0f, edgeSpawnCooldown - 0.03f * score);

        if (isEdgeCol && (globalTime - lastEdgeSpawnTime < dynamicEdgeCooldown)) {
            canSpawn = false; // skip this turn for fairness
        }

        if (canSpawn) {
            // Randomly choose GoodEgg or BadEgg (50/50 chance)
            Egg newEgg;
            newEgg.position = QPointF(float(col), 0.0f);
            newEgg.type = QRandomGenerator::global()->bounded(2) == 0 ? GoodEgg : BadEgg;
            eggs.append(newEgg);

            if (isEdgeCol)
                lastEdgeSpawnTime = globalTime; // record last edge spawn time
        }

        currentColumnIndex = (currentColumnIndex + 1) % dropColumns.size();
        globalSpawnTimer = 0.0f;
        spawnInterval = 2.0f + QRandomGenerator::global()->bounded(1.0f);
    }

    // --- Basket movement ---
    if (moveLeft && !moveRight)
        basketTargetVel = -basketMaxVel;
    else if (moveRight && !moveLeft)
        basketTargetVel = basketMaxVel;
    else
        basketTargetVel = 0.0f;

    float blend = 1.0f - qExp(-basketAccel * dt);
    basketXVelocity += (basketTargetVel - basketXVelocity) * blend;
    basket.setX(basket.x() + basketXVelocity * dt);
    basket.setX(std::clamp((float)basket.x(), 0.0f, float(cols - 1)));

    // --- Egg falling speed (scales with score) ---
    float fallSpeed = 3.0f + 0.15f * score;
    fallSpeed = std::min(fallSpeed, 25.0f); // cap at 25
    for (auto &egg : eggs)
        egg.position.setY(egg.position.y() + fallSpeed * dt);

    // --- Collision detection (Original blocky basket structure) ---
    QVector<Egg> newEggs;
    int bx = qRound(basket.x());
    int by = qRound(basket.y());

    // Original Basket Cell Structure from your initial code
    QVector<QPoint> basketCells = {
        {bx-2, by}, {bx-1, by}, {bx, by}, {bx+1, by}, {bx+2, by},
        {bx-1, by+1}, {bx, by+1}, {bx+1, by+1},
        {bx, by+2}
    };

    QVector<QRectF> basketRects;
    for (auto &cell : basketCells) {
        int cx = std::clamp(cell.x(), 0, cols-1);
        int cy = std::clamp(cell.y(), 0, rows-1);
        basketRects.push_back(QRectF(cx - 0.1f, cy - 0.1f, 1.2f, 1.2f));
    }

    for (auto &egg : eggs) {
        // 1. Missed Egg Check
        if (egg.position.y() >= rows - 1) {
            if (egg.type == GoodEgg) {
                // Miss a good egg: score -1 and lives -1
                score = std::max(0, score - 1);
                lives--;
                soundLose.play();
            }
            // Miss a bad egg: no penalty
            continue;
        }

        // 2. Catch Check
        QRectF eggRect(egg.position.x(), egg.position.y(), 1.0f, 1.0f);
        bool caught = false;
        for (auto &bRect : basketRects) {
            if (eggRect.intersects(bRect)) {
                if (egg.type == GoodEgg) {
                    score++; // Catch a good egg: score +1
                    soundCatch.play();
                } else { // BadEgg
                    score = std::max(0, score - 1); // Catch a bad egg: score -1 (lives are NOT minus)
                    soundLose.play();
                }
                caught = true;
                break;
            }
        }


        if (!caught)
            newEggs.push_back(egg);
    }

    eggs = newEggs;

    // --- Per-column delayed spawns ---
    for (int i = 0; i < dropColumns.size(); ++i) {
        bool hasEgg = false;
        for (auto &egg : eggs) {
            if (int(egg.position.x()) == dropColumns[i]) {
                hasEgg = true;
                break;
            }
        }

        if (!hasEgg) {
            columnTimers[i] += dt;
            if (columnTimers[i] >= columnDelays[i]) {
                Egg newEgg;
                newEgg.position = QPointF(float(dropColumns[i]), 0.0f);
                newEgg.type = QRandomGenerator::global()->bounded(2) == 0 ? GoodEgg : BadEgg;
                eggs.append(newEgg);
                columnTimers[i] = 0.0f;
                columnDelays[i] = 3.0f + QRandomGenerator::global()->bounded(2.0f);
            }
        }
    }
}

void MainWindow::drawGame(float alpha) {
    QPixmap framePix = background;
    QPainter painter(&framePix);

    auto snapToGrid = [&](float coord) { return qRound(coord) * grid_box; };

    // --- Interpolated basket render ---
    float basketRenderX = basket.x() + basketXVelocity * alpha * fixedDelta;
    int bx = qRound(basketRenderX);
    int by = qRound(basket.y());

    // Original Basket Cell Structure
    QVector<QPoint> basketCells = {
        {bx-2, by}, {bx-1, by}, {bx, by}, {bx+1, by}, {bx+2, by},
        {bx-1, by+1}, {bx, by+1}, {bx+1, by+1},
        {bx, by+2}
    };

    // Set basket color to Brown
    painter.setBrush(QColor(139, 69, 19));
    painter.setPen(Qt::NoPen);

    // Draw the basket using the original blocky cells (simple filled rectangles)
    for (auto &cell : basketCells) {
        int cx = std::clamp(cell.x(), 0, cols-1);
        int cy = std::clamp(cell.y(), 0, rows-1);
        QRectF rect(cx * grid_box, cy * grid_box, grid_box, grid_box);
        painter.fillRect(rect, QColor(139, 69, 19));
    }


    // --- Eggs render (Using Basic Ellipse Algorithm - FIX applied) ---
    float fallSpeed = 3.0f + 0.15f * score;
    fallSpeed = std::min(fallSpeed, 25.0f);

    painter.setPen(Qt::NoPen); // We'll set the brush inside the loop

    // Ellipse parameters (relative to the grid cell, 0 to grid_box)
    const float cx = grid_box / 2.0f;
    const float cy = grid_box / 2.0f;
    // Set a slight ellipse (a=10, b=12) to make it look less circular
    const float a2 = 10.0f * 10.0f; // horizontal radius squared
    const float b2 = 12.0f * 12.0f; // vertical radius squared

    // Pixel size for better visibility
    const int PIXEL_SIZE = 4; // Draw a 4x4 pixel block for each point

    for (auto &egg : eggs) {
        float eggRenderY = egg.position.y() + fallSpeed * alpha * fixedDelta;
        int startX = snapToGrid(egg.position.x());
        int startY = snapToGrid(eggRenderY);

        // Set egg color based on type
        if (egg.type == GoodEgg) {
            painter.setBrush(Qt::white);
        } else {
            painter.setBrush(QColor(128, 0, 32)); // Burgundy color
        }

        // Manual Scanline/Pixel Check for Ellipse
        for (int x = 0; x < grid_box; x += PIXEL_SIZE) { // Step by PIXEL_SIZE
            for (int y = 0; y < grid_box; y += PIXEL_SIZE) { // Step by PIXEL_SIZE
                // Check if the center of the current block (x, y) falls within the ellipse equation:
                float dx = (x + PIXEL_SIZE/2.0f) - cx;
                float dy = (y + PIXEL_SIZE/2.0f) - cy;

                if ((dx * dx / a2) + (dy * dy / b2) <= 1.0f) {
                    // FIX: Draw a small filled rectangle instead of a single point
                    painter.fillRect(startX + x, startY + y, PIXEL_SIZE, PIXEL_SIZE, painter.brush().color());
                }
            }
        }
    }

    // --- Score / Lives HUD ---
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 10));
    painter.drawText(10, 20,
                     QString("Score: %1   Lives: %2").arg(score).arg(lives));

    painter.end();
    ui->frame->setPixmap(framePix);
}
