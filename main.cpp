/*
 * Rubik's Cube - Phase 2: 3x3x3 Rubik's Cube with 27 Pieces
 * Computer Graphics Final Project
 * 
 * ========================================================================
 * DIAGNOSIS: ĐÚNG - Double Rotation Bug
 * ========================================================================
 * 
 * NGUYÊN NHÂN:
 * Code cũ đang xoay orientation của TẤT CẢ 8 pieces (trừ center) TRƯỚC KHI
 * biết mapping. Nhưng thực tế, chỉ những pieces THỰC SỰ DI CHUYỂN (mapping[i] != i)
 * mới cần xoay orientation. Điều này gây ra "double rotation":
 * - Pieces không di chuyển vẫn bị xoay orientation → sai
 * - Sau 2 lần xoay, pattern lặp lại → đúng
 * - Pattern: F1 sai, F2 đúng, F3 sai, F4 đúng (về ban đầu)
 * 
 * GIẢI PHÁP:
 * 1. Tính mapping trước
 * 2. Chỉ xoay orientation của pieces THỰC SỰ DI CHUYỂN (mapping[i] != i và i != 4)
 * 3. Sau đó mới swap colors từ rotated backup
 * 
 * KẾT QUẢ:
 * - F^4 = identity (F 4 lần về đúng trạng thái ban đầu)
 * - Mỗi lần F đều cho kết quả đúng
 * 
 * ========================================================================
 * 
 * Compilation command (Windows/MinGW - PowerShell):
 * g++ -std=c++98 -Wall -Wextra -O2 main.cpp -lfreeglut -lopengl32 -lglu32 -o rubik.exe
 * 
 * One-line compile + run (PowerShell):
 * g++ -std=c++98 -Wall -Wextra -O2 main.cpp -lfreeglut -lopengl32 -lglu32 -o rubik.exe; if ($?) { .\rubik.exe }
 * 
 * Or separate commands:
 * g++ -std=c++98 -Wall -Wextra -O2 main.cpp -lfreeglut -lopengl32 -lglu32 -o rubik.exe
 * .\rubik.exe
 * 
 * Compilation command (Linux):
 * g++ -std=c++98 -Wall -Wextra -O2 main.cpp -lglut -lGLU -lGL -lm -o rubik
 */

#include <GL/glut.h>
#include <cmath>
#include <iostream>
#include <cstdlib> // for system("pause")
#include <cstdio>  // for fprintf debug logging
#include <ctime>   // for timestamp
#include <cstring> // for memcpy
#include <cctype>  // for toupper

#if defined(_MSC_VER) && !defined(snprintf)
#define snprintf _snprintf
#endif

// Window dimensions
int windowWidth = 800;
int windowHeight = 600;

// Camera rotation angles (degrees) - accumulate rotations
float cameraAngleX = 0.0f;
float cameraAngleY = 0.0f;

// Mouse tracking for arcball camera control
bool isDragging = false;
int lastMouseX = 0;
int lastMouseY = 0;

// Debug log file
FILE* g_logFile = NULL;
clock_t g_logStartClock = 0; // used to compute relative timestamps

// Rotation sensitivity (adjust for smooth control)
const float ROTATION_SENSITIVITY = 0.3f;

// Keyboard rotation speed (degrees per key press)
const float KEYBOARD_ROTATION_SPEED = 5.0f;

// Camera distance from origin
const float CAMERA_DISTANCE = 8.0f;

// Rubik's Cube constants
const float PIECE_SIZE = 0.9f;  // Size of each cube piece
const float GAP_SIZE = 0.1f;    // Gap between pieces

// Standard Rubik's Cube colors
// Face indices: 0=Front, 1=Back, 2=Left, 3=Right, 4=Up, 5=Down
const float COLOR_WHITE[] = {1.0f, 1.0f, 1.0f};   // Up
const float COLOR_YELLOW[] = {1.0f, 1.0f, 0.0f};  // Down
const float COLOR_RED[] = {1.0f, 0.0f, 0.0f};     // Front
const float COLOR_ORANGE[] = {1.0f, 0.5f, 0.0f};  // Back
const float COLOR_GREEN[] = {0.0f, 1.0f, 0.0f};   // Right
const float COLOR_BLUE[] = {0.0f, 0.0f, 1.0f};    // Left
const float COLOR_BLACK[] = {0.1f, 0.1f, 0.1f};   // Hidden faces

// CubePiece structure - represents one piece of the Rubik's Cube
struct CubePiece {
    int position[3];        // Grid position: x,y,z in {-1, 0, +1}
    float colors[6][3];    // 6 faces RGB: [0=Front,1=Back,2=Left,3=Right,4=Up,5=Down]
    bool isVisible;         // Whether this piece is visible (all pieces visible in Phase 2)
};

// RubikCube structure - contains all 27 pieces
struct RubikCube {
    CubePiece pieces[27];  // Fixed array 3x3x3 = 27 pieces
    float pieceSize;        // Size of each piece
    float gapSize;         // Gap between pieces
};


// Global Rubik's Cube instance
RubikCube g_rubikCube;

// Animation state (for future use)
// Face orientation enum
enum Face {
    FRONT = 0,  // Red
    BACK = 1,   // Orange
    LEFT = 2,   // Blue
    RIGHT = 3,  // Green
    UP = 4,     // White
    DOWN = 5    // Yellow
};

const float ROTATION_SPEED_DEG_PER_SEC = 360.0f;
const int MOVE_QUEUE_CAPACITY = 20;

struct RotationAnimation {
    bool isActive;
    Face face;
    bool clockwise;
    bool isScrambleMove;
    float currentAngle;
    float targetAngle;
    float speed;
    float displayAngle;
    int affectedIndices[9];
};

struct MoveQueue {
    int moves[MOVE_QUEUE_CAPACITY];
    bool dirs[MOVE_QUEUE_CAPACITY];
    bool scrambleFlags[MOVE_QUEUE_CAPACITY];
    int count;
    int head;
};

RotationAnimation g_animation = {
    false,
    FRONT,
    true,
    false,
    0.0f,
    90.0f,
    ROTATION_SPEED_DEG_PER_SEC,
    0.0f,
    {-1, -1, -1, -1, -1, -1, -1, -1, -1}
};

MoveQueue g_moveQueue = {{0}, {false}, {false}, 0, 0};
int g_lastTimeMs = 0;
bool g_keyHeld[256] = {false};

enum TimerState {
    TIMER_IDLE = 0,
    TIMER_READY,
    TIMER_RUNNING,
    TIMER_STOPPED
};

struct SpeedTimer {
    TimerState state;
    float startTime;
    float endTime;
    int moveCount;
    float currentTime;
    float tps;
    float lastSampleTime;
};

SpeedTimer g_timer = {TIMER_IDLE, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f};
int g_scrambleMovesPending = 0;

void resetTimerState() {
    g_timer.state = TIMER_IDLE;
    g_timer.startTime = 0.0f;
    g_timer.endTime = 0.0f;
    g_timer.moveCount = 0;
    g_timer.currentTime = 0.0f;
    g_timer.tps = 0.0f;
    g_timer.lastSampleTime = 0.0f;
}

void armTimerForSolve() {
    g_timer.state = TIMER_READY;
    g_timer.startTime = 0.0f;
    g_timer.endTime = 0.0f;
    g_timer.moveCount = 0;
    g_timer.currentTime = 0.0f;
    g_timer.tps = 0.0f;
    g_timer.lastSampleTime = 0.0f;
}

void handleScrambleMoveCompletion(bool wasScrambleMove) {
    if (!wasScrambleMove || g_scrambleMovesPending <= 0) {
        return;
    }
    g_scrambleMovesPending--;
    if (g_scrambleMovesPending == 0) {
        armTimerForSolve();
    }
}

// Current front face orientation
Face currentFrontFace = FRONT;

// Dynamic rotation axes based on current front face
// These are 3D vectors (x, y, z)
float verticalAxis[3];    // Axis for UP/DOWN rotation
float horizontalAxis[3];   // Axis for LEFT/RIGHT rotation

// Initialize log file
void initLogFile() {
    g_logFile = fopen("rubik_debug.log", "w");
    if (g_logFile == NULL) {
        std::cerr << "Warning: Cannot open log file rubik_debug.log" << std::endl;
        return;
    }
    g_logStartClock = clock();
    
    // Write header with timestamp
    time_t rawTime;
    struct tm* timeInfo;
    char timeStr[80];
    
    time(&rawTime);
    timeInfo = localtime(&rawTime);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeInfo);
    
    fprintf(g_logFile, "=== Rubik's Cube Debug Log ===\n");
    fprintf(g_logFile, "Started: %s\n", timeStr);
    fprintf(g_logFile, "==============================\n\n");
    fflush(g_logFile);
}

double getLogTimestampMs() {
    if (g_logStartClock == 0) {
        return 0.0;
    }
    clock_t current = clock();
    return (double)(current - g_logStartClock) * 1000.0 / CLOCKS_PER_SEC;
}

// Close log file
void closeLogFile() {
    if (g_logFile != NULL) {
        fprintf(g_logFile, "\n=== Log End ===\n");
        fclose(g_logFile);
        g_logFile = NULL;
    }
}

// Clamp angle between min and max
float clampAngle(float angle, float minAngle, float maxAngle) {
    if (angle < minAngle) {
        return minAngle;
    }
    if (angle > maxAngle) {
        return maxAngle;
    }
    return angle;
}

// Matrix multiplication: result = a * b
void matrixMultiply(float result[3][3], const float a[3][3], const float b[3][3]) {
    int i, j, k;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            result[i][j] = 0.0f;
            for (k = 0; k < 3; k++) {
                result[i][j] += a[i][k] * b[k][j];
            }
        }
    }
}

// Convert axis-angle rotation to 3x3 rotation matrix (Rodrigues' rotation formula)
void axisAngleToMatrix(float m[3][3], const float axis[3], float angleDegrees) {
    float angleRad = angleDegrees * 3.14159f / 180.0f;
    float c = cos(angleRad);
    float s = sin(angleRad);
    float t = 1.0f - c;
    
    // Normalize axis
    float length = sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    float x, y, z;
    if (length > 0.0001f) {
        x = axis[0] / length;
        y = axis[1] / length;
        z = axis[2] / length;
    } else {
        x = axis[0];
        y = axis[1];
        z = axis[2];
    }
    
    // Rodrigues' rotation formula
    m[0][0] = t * x * x + c;
    m[0][1] = t * x * y - s * z;
    m[0][2] = t * x * z + s * y;
    
    m[1][0] = t * x * y + s * z;
    m[1][1] = t * y * y + c;
    m[1][2] = t * y * z - s * x;
    
    m[2][0] = t * x * z - s * y;
    m[2][1] = t * y * z + s * x;
    m[2][2] = t * z * z + c;
}

void rotateVectorAroundAxis(const float axis[3], float angleDegrees,
                            float& x, float& y, float& z) {
    if (fabs(angleDegrees) < 0.0001f) {
        return;
    }
    float rot[3][3];
    axisAngleToMatrix(rot, axis, angleDegrees);
    float rx = rot[0][0] * x + rot[0][1] * y + rot[0][2] * z;
    float ry = rot[1][0] * x + rot[1][1] * y + rot[1][2] * z;
    float rz = rot[2][0] * x + rot[2][1] * y + rot[2][2] * z;
    x = rx;
    y = ry;
    z = rz;
}

Face getOppositeFace(Face face) {
    switch (face) {
        case FRONT: return BACK;
        case BACK: return FRONT;
        case LEFT: return RIGHT;
        case RIGHT: return LEFT;
        case UP: return DOWN;
        case DOWN: return UP;
        default: return FRONT;
    }
}

struct ViewFaceMapping {
    Face front;
    Face back;
    Face left;
    Face right;
    Face up;
    Face down;
};

void applyCurrentViewRotation(float& x, float& y, float& z) {
    // Apply rotations in the same *effect* order as display() to match OpenGL's right-to-left matrix multiply
    rotateVectorAroundAxis(verticalAxis, cameraAngleX, x, y, z);   // display()'s second glRotate executes first on geometry
    rotateVectorAroundAxis(horizontalAxis, cameraAngleY, x, y, z); // then yaw around the horizontal axis
}

void computeViewFaceMapping(ViewFaceMapping& mapping) {
    const float normals[6][3] = {
        {0.0f, 0.0f, 1.0f},   // FRONT
        {0.0f, 0.0f, -1.0f},  // BACK
        {-1.0f, 0.0f, 0.0f},  // LEFT
        {1.0f, 0.0f, 0.0f},   // RIGHT
        {0.0f, 1.0f, 0.0f},   // UP
        {0.0f, -1.0f, 0.0f}   // DOWN
    };
    const Face faces[6] = {FRONT, BACK, LEFT, RIGHT, UP, DOWN};
    float rotated[6][3];
    for (int i = 0; i < 6; i++) {
        rotated[i][0] = normals[i][0];
        rotated[i][1] = normals[i][1];
        rotated[i][2] = normals[i][2];
        applyCurrentViewRotation(rotated[i][0], rotated[i][1], rotated[i][2]);
    }
    float bestFrontDot = -1000.0f;
    float bestUpDot = -1000.0f;
    int frontIdx = 0;
    int upIdx = 4;
    const float viewFront[3] = {0.0f, 0.0f, 1.0f};
    const float viewUp[3] = {0.0f, 1.0f, 0.0f};
    for (int i = 0; i < 6; i++) {
        float dotFront = rotated[i][0] * viewFront[0] + rotated[i][1] * viewFront[1] + rotated[i][2] * viewFront[2];
        if (dotFront > bestFrontDot) {
            bestFrontDot = dotFront;
            frontIdx = i;
        }
    }
    for (int i = 0; i < 6; i++) {
        if (i == frontIdx || faces[i] == getOppositeFace(faces[frontIdx])) {
            continue;
        }
        float dotUp = rotated[i][0] * viewUp[0] + rotated[i][1] * viewUp[1] + rotated[i][2] * viewUp[2];
        if (dotUp > bestUpDot) {
            bestUpDot = dotUp;
            upIdx = i;
        }
    }
    float frontVec[3] = {rotated[frontIdx][0], rotated[frontIdx][1], rotated[frontIdx][2]};
    float upVec[3] = {rotated[upIdx][0], rotated[upIdx][1], rotated[upIdx][2]};
    // Ensure upVec is orthogonal to frontVec
    float frontDotUp = frontVec[0] * upVec[0] + frontVec[1] * upVec[1] + frontVec[2] * upVec[2];
    if (fabs(frontDotUp) > 0.9f) {
        // Choose another candidate for up
        bestUpDot = -1000.0f;
        for (int i = 0; i < 6; i++) {
            if (i == frontIdx || faces[i] == getOppositeFace(faces[frontIdx])) {
                continue;
            }
            float dotUp = rotated[i][0] * viewUp[0] + rotated[i][1] * viewUp[1] + rotated[i][2] * viewUp[2];
            if (dotUp > bestUpDot && fabs(rotated[i][0] * frontVec[0] + rotated[i][1] * frontVec[1] + rotated[i][2] * frontVec[2]) < 0.1f) {
                bestUpDot = dotUp;
                upIdx = i;
                upVec[0] = rotated[i][0];
                upVec[1] = rotated[i][1];
                upVec[2] = rotated[i][2];
            }
        }
    }
    // Compute right vector via cross product (up × front) to maintain a right-handed basis
    float rightVec[3];
    rightVec[0] = upVec[1] * frontVec[2] - upVec[2] * frontVec[1];
    rightVec[1] = upVec[2] * frontVec[0] - upVec[0] * frontVec[2];
    rightVec[2] = upVec[0] * frontVec[1] - upVec[1] * frontVec[0];
    float rightLen = sqrt(rightVec[0] * rightVec[0] + rightVec[1] * rightVec[1] + rightVec[2] * rightVec[2]);
    if (rightLen > 0.0001f) {
        rightVec[0] /= rightLen;
        rightVec[1] /= rightLen;
        rightVec[2] /= rightLen;
    } else {
        rightVec[0] = 1.0f;
        rightVec[1] = 0.0f;
        rightVec[2] = 0.0f;
    }
    int rightIdx = 3;
    float bestRightDot = -1000.0f;
    for (int i = 0; i < 6; i++) {
        float dotRight = rotated[i][0] * rightVec[0] + rotated[i][1] * rightVec[1] + rotated[i][2] * rightVec[2];
        if (dotRight > bestRightDot) {
            bestRightDot = dotRight;
            rightIdx = i;
        }
    }
    mapping.front = faces[frontIdx];
    mapping.back = getOppositeFace(mapping.front);
    mapping.up = faces[upIdx];
    mapping.down = getOppositeFace(mapping.up);
    mapping.right = faces[rightIdx];
    mapping.left = getOppositeFace(mapping.right);
}

// Update rotation axes based on current front face
void updateRotationAxes() {
    // Reset axes
    verticalAxis[0] = 0.0f;
    verticalAxis[1] = 0.0f;
    verticalAxis[2] = 0.0f;
    horizontalAxis[0] = 0.0f;
    horizontalAxis[1] = 0.0f;
    horizontalAxis[2] = 0.0f;
    
    switch (currentFrontFace) {
        case FRONT: // Red front, White up, Yellow down
            // UP/DOWN rotates around X-axis (left-right axis)
            verticalAxis[0] = 1.0f;
            verticalAxis[1] = 0.0f;
            verticalAxis[2] = 0.0f;
            // LEFT/RIGHT rotates around Y-axis (up-down axis)
            horizontalAxis[0] = 0.0f;
            horizontalAxis[1] = 1.0f;
            horizontalAxis[2] = 0.0f;
            break;
            
        case RIGHT: // Green front, White up, Yellow down
            // UP/DOWN rotates around Z-axis (front-back axis)
            verticalAxis[0] = 0.0f;
            verticalAxis[1] = 0.0f;
            verticalAxis[2] = 1.0f;
            // LEFT/RIGHT rotates around X-axis (left-right axis)
            horizontalAxis[0] = 1.0f;
            horizontalAxis[1] = 0.0f;
            horizontalAxis[2] = 0.0f;
            break;
            
        case BACK: // Orange front, White up, Yellow down
            // UP/DOWN rotates around -X axis
            verticalAxis[0] = -1.0f;
            verticalAxis[1] = 0.0f;
            verticalAxis[2] = 0.0f;
            // LEFT/RIGHT rotates around -Y axis
            horizontalAxis[0] = 0.0f;
            horizontalAxis[1] = -1.0f;
            horizontalAxis[2] = 0.0f;
            break;
            
        case LEFT: // Blue front, White up, Yellow down
            // UP/DOWN rotates around -Z axis
            verticalAxis[0] = 0.0f;
            verticalAxis[1] = 0.0f;
            verticalAxis[2] = -1.0f;
            // LEFT/RIGHT rotates around -X axis
            horizontalAxis[0] = -1.0f;
            horizontalAxis[1] = 0.0f;
            horizontalAxis[2] = 0.0f;
            break;
            
        case UP: // White front, Red right, Orange left
            // UP/DOWN rotates around Y-axis
            verticalAxis[0] = 0.0f;
            verticalAxis[1] = 1.0f;
            verticalAxis[2] = 0.0f;
            // LEFT/RIGHT rotates around Z-axis
            horizontalAxis[0] = 0.0f;
            horizontalAxis[1] = 0.0f;
            horizontalAxis[2] = 1.0f;
            break;
            
        case DOWN: // Yellow front, Orange right, Red left
            // UP/DOWN rotates around -Y axis
            verticalAxis[0] = 0.0f;
            verticalAxis[1] = -1.0f;
            verticalAxis[2] = 0.0f;
            // LEFT/RIGHT rotates around -Z axis
            horizontalAxis[0] = 0.0f;
            horizontalAxis[1] = 0.0f;
            horizontalAxis[2] = -1.0f;
            break;
    }
    
    // Log face change
    if (g_logFile != NULL) {
        const char* faceNames[] = {"FRONT", "BACK", "LEFT", "RIGHT", "UP", "DOWN"};
        fprintf(g_logFile, "FACE CHANGE: %s | verticalAxis=[%.1f,%.1f,%.1f] horizontalAxis=[%.1f,%.1f,%.1f]\n",
                faceNames[currentFrontFace],
                verticalAxis[0], verticalAxis[1], verticalAxis[2],
                horizontalAxis[0], horizontalAxis[1], horizontalAxis[2]);
        fflush(g_logFile);
    }
}

// Rotate around arbitrary axis using glRotatef
void rotateAroundAxis(const float axis[3], float angle) {
    // Normalize axis vector
    float length = sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    if (length > 0.0001f) {
        float nx = axis[0] / length;
        float ny = axis[1] / length;
        float nz = axis[2] / length;
        
        // Apply rotation using glRotatef
        glRotatef(angle, nx, ny, nz);
    }
}

// Reset rotation angles
void resetRotationAngles() {
    cameraAngleX = 0.0f;
    cameraAngleY = 0.0f;
}

// Forward declarations
void initRubikCube();
void rotatePieceOrientation(int pieceIndex, int axis, bool clockwise);
Face getAbsoluteFace(int relativeFace);
void rotateFace(int face, bool clockwise);
void startRotation(Face face, bool clockwise, bool isScrambleMove = false);
void updateAnimation(float deltaTime);
bool isPieceInAnimation(int pieceIndex);
float easeInOutCubic(float t);
void idle();
void cancelAnimationAndQueue();
void keyboardUp(unsigned char key, int x, int y);
void onMoveStarted();
bool isCubeSolved();
void updateTimer();
void resetTimerState();
void armTimerForSolve();
void handleScrambleMoveCompletion(bool wasScrambleMove);
void renderBitmapString(float x, float y, void* font, const char* string);
void displayTimerOverlay();

// Convert position (i,j,k) to array index
// Loop order: k=1,0,-1; j=-1,0,1; i=-1,0,1
int positionToIndex(int i, int j, int k) {
    // k=1: indices 0-8, k=0: indices 9-17, k=-1: indices 18-26
    int kOffset = (k == 1) ? 0 : (k == 0) ? 9 : 18;
    int jOffset = (j + 1) * 3;
    int iOffset = (i + 1);
    return kOffset + jOffset + iOffset;
}

// Get 9 indices of pieces on a face
void getFaceIndices(int face, int indices[9]) {
    int idx = 0;
    int i, j, k;
    
    switch (face) {
        case FRONT: // Z = 1
            k = 1;
            for (j = -1; j <= 1; j++) {
                for (i = -1; i <= 1; i++) {
                    indices[idx++] = positionToIndex(i, j, k);
                }
            }
            break;
            
        case BACK: // Z = -1
            k = -1;
            for (j = -1; j <= 1; j++) {
                for (i = -1; i <= 1; i++) {
                    indices[idx++] = positionToIndex(i, j, k);
                }
            }
            break;
            
        case LEFT: // X = -1
            i = -1;
            for (j = -1; j <= 1; j++) {
                for (k = 1; k >= -1; k--) {
                    indices[idx++] = positionToIndex(i, j, k);
                }
            }
            break;
            
        case RIGHT: // X = 1
            i = 1;
            for (j = -1; j <= 1; j++) {
                for (k = 1; k >= -1; k--) {
                    indices[idx++] = positionToIndex(i, j, k);
                }
            }
            break;
            
        case UP: // Y = 1
            j = 1;
            for (k = 1; k >= -1; k--) {
                for (i = -1; i <= 1; i++) {
                    indices[idx++] = positionToIndex(i, j, k);
                }
            }
            break;
            
        case DOWN: // Y = -1
            j = -1;
            for (k = 1; k >= -1; k--) {
                for (i = -1; i <= 1; i++) {
                    indices[idx++] = positionToIndex(i, j, k);
                }
            }
            break;
    }
}

int encodePositionKey(int x, int y, int z) {
    return (x + 1) * 9 + (y + 1) * 3 + (z + 1);
}

void rotateCoordinates(int axis, int axisSign, bool clockwise,
                       int x, int y, int z,
                       int& rx, int& ry, int& rz) {
    int angleSign = clockwise ? -1 : 1;  // -90 for CW, +90 for CCW when looking along +axis
    angleSign *= axisSign;               // adjust for faces whose normal points opposite
    switch (axis) {
        case 0: // X-axis
            rx = x;
            if (angleSign > 0) {
                ry = -z;
                rz = y;
            } else {
                ry = z;
                rz = -y;
            }
            break;
        case 1: // Y-axis
            ry = y;
            if (angleSign > 0) {
                rz = -x;
                rx = z;
            } else {
                rz = x;
                rx = -z;
            }
            break;
        case 2: // Z-axis
            rz = z;
            if (angleSign > 0) {
                rx = -y;
                ry = x;
            } else {
                rx = y;
                ry = -x;
            }
            break;
        default:
            rx = x;
            ry = y;
            rz = z;
            break;
    }
}


// Rotate positions of 9 pieces on a face (clockwise 90°)
// CRITICAL: Only swap colors, NEVER swap position (position is fixed grid coords)
// FIXED: Only rotate orientation of pieces that ACTUALLY MOVE (mapping[i] != i)
void rotatePositions(int face, bool clockwise) {
    int indices[9];
    getFaceIndices(face, indices);
    
    // Backup ONLY colors (6 faces × 3 RGB per piece)
    float backupColors[9][6][3];
    int i, f, c;
    for (i = 0; i < 9; i++) {
        for (f = 0; f < 6; f++) {
            for (c = 0; c < 3; c++) {
                backupColors[i][f][c] = g_rubikCube.pieces[indices[i]].colors[f][c];
            }
        }
    }
    
    // Determine rotation axis and sign based on face
    int rotationAxis;
    int axisSign = 1;
    switch (face) {
        case FRONT:
        case BACK:
            rotationAxis = 2;  // Z-axis
            axisSign = (face == FRONT) ? 1 : -1;
            break;
        case LEFT:
        case RIGHT:
            rotationAxis = 0;  // X-axis
            axisSign = (face == RIGHT) ? 1 : -1;
            break;
        case UP:
        case DOWN:
            rotationAxis = 1;  // Y-axis
            axisSign = (face == UP) ? 1 : -1;
            break;
        default:
            rotationAxis = 2;  // Default to Z-axis
            break;
    }
    
    // Calculate mapping FIRST to know which pieces actually move
    int mapping[9];
    int keyToSlot[27];
    for (i = 0; i < 27; i++) {
        keyToSlot[i] = -1;
    }
    for (i = 0; i < 9; i++) {
        const CubePiece& piece = g_rubikCube.pieces[indices[i]];
        int key = encodePositionKey(piece.position[0], piece.position[1], piece.position[2]);
        keyToSlot[key] = i;
    }
    for (i = 0; i < 9; i++) {
        const CubePiece& piece = g_rubikCube.pieces[indices[i]];
        int rx, ry, rz;
        rotateCoordinates(rotationAxis, axisSign, clockwise,
                          piece.position[0], piece.position[1], piece.position[2],
                          rx, ry, rz);
        int key = encodePositionKey(rx, ry, rz);
        int destSlot = keyToSlot[key];
        mapping[destSlot] = i;
    }
    
    // Apply rotation: for each destination, copy SOURCE piece and rotate its orientation
    // CRITICAL: All pieces except center (slot 4) need orientation rotation when face rotates
    int j;
    int srcIdx;
    float temp[3];
    
    for (i = 0; i < 9; i++) {
        srcIdx = mapping[i];  // Source piece that will move to destination slot i
        
        // Copy colors from source to destination
        for (f = 0; f < 6; f++) {
            for (c = 0; c < 3; c++) {
                g_rubikCube.pieces[indices[i]].colors[f][c] = backupColors[srcIdx][f][c];
            }
        }
        
        // Rotate orientation for all pieces EXCEPT center piece (slot 4)
        // Even if piece stays in same slot (srcIdx == i), it still rotates with the face
        if (i != 4) {
            // Rotate orientation of the DESTINATION piece (which now has SOURCE colors)
            CubePiece* p = &g_rubikCube.pieces[indices[i]];
            bool orientationClockwise = clockwise;
            if (axisSign < 0) {
                orientationClockwise = !orientationClockwise;
            }
            
            if (orientationClockwise) {
                if (rotationAxis == 2) {  // Z-axis (FRONT/BACK)
                    // U→R→D→L→U (clockwise around Z)
                    for (j = 0; j < 3; j++) temp[j] = p->colors[4][j];
                    for (j = 0; j < 3; j++) p->colors[4][j] = p->colors[2][j];  // L→U
                    for (j = 0; j < 3; j++) p->colors[2][j] = p->colors[5][j];  // D→L
                    for (j = 0; j < 3; j++) p->colors[5][j] = p->colors[3][j];  // R→D
                    for (j = 0; j < 3; j++) p->colors[3][j] = temp[j];  // U→R
                } else if (rotationAxis == 0) {  // X-axis (LEFT/RIGHT)
                    // F→U→B→D→F (clockwise around X)
                    for (j = 0; j < 3; j++) temp[j] = p->colors[0][j];
                    for (j = 0; j < 3; j++) p->colors[0][j] = p->colors[5][j];  // D→F
                    for (j = 0; j < 3; j++) p->colors[5][j] = p->colors[1][j];  // B→D
                    for (j = 0; j < 3; j++) p->colors[1][j] = p->colors[4][j];  // U→B
                    for (j = 0; j < 3; j++) p->colors[4][j] = temp[j];  // F→U
                } else if (rotationAxis == 1) {  // Y-axis (UP/DOWN)
                    // R→F, B→R, L→B, F→L (clockwise around Y)
                    for (j = 0; j < 3; j++) temp[j] = p->colors[0][j];
                    for (j = 0; j < 3; j++) p->colors[0][j] = p->colors[3][j];  // R→F
                    for (j = 0; j < 3; j++) p->colors[3][j] = p->colors[1][j];  // B→R
                    for (j = 0; j < 3; j++) p->colors[1][j] = p->colors[2][j];  // L→B
                    for (j = 0; j < 3; j++) p->colors[2][j] = temp[j];  // F→L
                }
            } else {
                if (rotationAxis == 2) {  // Z-axis (FRONT/BACK)
                    // U→L→D→R→U (counter-clockwise around Z)
                    for (j = 0; j < 3; j++) temp[j] = p->colors[4][j];
                    for (j = 0; j < 3; j++) p->colors[4][j] = p->colors[3][j];  // R→U
                    for (j = 0; j < 3; j++) p->colors[3][j] = p->colors[5][j];  // D→R
                    for (j = 0; j < 3; j++) p->colors[5][j] = p->colors[2][j];  // L→D
                    for (j = 0; j < 3; j++) p->colors[2][j] = temp[j];  // U→L
                } else if (rotationAxis == 0) {  // X-axis (LEFT/RIGHT)
                    // F→D→B→U→F (counter-clockwise around X)
                    for (j = 0; j < 3; j++) temp[j] = p->colors[0][j];
                    for (j = 0; j < 3; j++) p->colors[0][j] = p->colors[4][j];  // U→F
                    for (j = 0; j < 3; j++) p->colors[4][j] = p->colors[1][j];  // B→U
                    for (j = 0; j < 3; j++) p->colors[1][j] = p->colors[5][j];  // D→B
                    for (j = 0; j < 3; j++) p->colors[5][j] = temp[j];  // F→D
                } else if (rotationAxis == 1) {  // Y-axis (UP/DOWN)
                    // L→F, B→L, R→B, F→R (counter-clockwise around Y)
                    for (j = 0; j < 3; j++) temp[j] = p->colors[0][j];
                    for (j = 0; j < 3; j++) p->colors[0][j] = p->colors[2][j];  // L→F
                    for (j = 0; j < 3; j++) p->colors[2][j] = p->colors[1][j];  // B→L
                    for (j = 0; j < 3; j++) p->colors[1][j] = p->colors[3][j];  // R→B
                    for (j = 0; j < 3; j++) p->colors[3][j] = temp[j];  // F→R
                }
            }
        }
        // position[] stays at original grid coords - NEVER MODIFIED
    }
    
    // Log rotation
    if (g_logFile != NULL) {
        const char* faceNames[] = {"FRONT", "BACK", "LEFT", "RIGHT", "UP", "DOWN"};
        fprintf(g_logFile, "ROTATE %s %s: colors swapped, positions preserved\n",
                faceNames[face], clockwise ? "CW" : "CCW");
        fflush(g_logFile);
    }
}

// Rotate piece orientation (rotate colors around axis when piece moves)
// This simulates real Rubik's Cube physics: when piece moves, its colors rotate
void rotatePieceOrientation(int pieceIndex, int axis, bool clockwise) {
    CubePiece* p = &g_rubikCube.pieces[pieceIndex];
    float temp[3];
    int i;
    
    // Face indices: 0=Front, 1=Back, 2=Left, 3=Right, 4=Up, 5=Down
    if (axis == 2) {  // Z-axis (FRONT/BACK rotation)
        if (clockwise) {
            // Clockwise around Z: U(4)→R(3)→D(5)→L(2)→U(4)
            // Backup U
            for (i = 0; i < 3; i++) {
                temp[i] = p->colors[4][i];
            }
            // L→U
            for (i = 0; i < 3; i++) {
                p->colors[4][i] = p->colors[2][i];
            }
            // D→L
            for (i = 0; i < 3; i++) {
                p->colors[2][i] = p->colors[5][i];
            }
            // R→D
            for (i = 0; i < 3; i++) {
                p->colors[5][i] = p->colors[3][i];
            }
            // U→R
            for (i = 0; i < 3; i++) {
                p->colors[3][i] = temp[i];
            }
        } else {
            // Counter-clockwise around Z: U(4)→L(2)→D(5)→R(3)→U(4)
            // Backup U
            for (i = 0; i < 3; i++) {
                temp[i] = p->colors[4][i];
            }
            // R→U
            for (i = 0; i < 3; i++) {
                p->colors[4][i] = p->colors[3][i];
            }
            // D→R
            for (i = 0; i < 3; i++) {
                p->colors[3][i] = p->colors[5][i];
            }
            // L→D
            for (i = 0; i < 3; i++) {
                p->colors[5][i] = p->colors[2][i];
            }
            // U→L
            for (i = 0; i < 3; i++) {
                p->colors[2][i] = temp[i];
            }
        }
    } else if (axis == 0) {  // X-axis (LEFT/RIGHT rotation)
        if (clockwise) {
            // Clockwise around X: F(0)→U(4)→B(1)→D(5)→F(0)
            // Backup F
            for (i = 0; i < 3; i++) {
                temp[i] = p->colors[0][i];
            }
            // D→F
            for (i = 0; i < 3; i++) {
                p->colors[0][i] = p->colors[5][i];
            }
            // B→D
            for (i = 0; i < 3; i++) {
                p->colors[5][i] = p->colors[1][i];
            }
            // U→B
            for (i = 0; i < 3; i++) {
                p->colors[1][i] = p->colors[4][i];
            }
            // F→U
            for (i = 0; i < 3; i++) {
                p->colors[4][i] = temp[i];
            }
        } else {
            // Counter-clockwise around X: F(0)→D(5)→B(1)→U(4)→F(0)
            // Backup F
            for (i = 0; i < 3; i++) {
                temp[i] = p->colors[0][i];
            }
            // U→F
            for (i = 0; i < 3; i++) {
                p->colors[0][i] = p->colors[4][i];
            }
            // B→U
            for (i = 0; i < 3; i++) {
                p->colors[4][i] = p->colors[1][i];
            }
            // D→B
            for (i = 0; i < 3; i++) {
                p->colors[1][i] = p->colors[5][i];
            }
            // F→D
            for (i = 0; i < 3; i++) {
                p->colors[5][i] = temp[i];
            }
        }
    } else if (axis == 1) {  // Y-axis (UP/DOWN rotation)
        if (clockwise) {
            // Clockwise around Y: F(0)→R(3)→B(1)→L(2)→F(0)
            // Backup F
            for (i = 0; i < 3; i++) {
                temp[i] = p->colors[0][i];
            }
            // L→F
            for (i = 0; i < 3; i++) {
                p->colors[0][i] = p->colors[2][i];
            }
            // B→L
            for (i = 0; i < 3; i++) {
                p->colors[2][i] = p->colors[1][i];
            }
            // R→B
            for (i = 0; i < 3; i++) {
                p->colors[1][i] = p->colors[3][i];
            }
            // F→R
            for (i = 0; i < 3; i++) {
                p->colors[3][i] = temp[i];
            }
        } else {
            // Counter-clockwise around Y: F(0)→L(2)→B(1)→R(3)→F(0)
            // Backup F
            for (i = 0; i < 3; i++) {
                temp[i] = p->colors[0][i];
            }
            // R→F
            for (i = 0; i < 3; i++) {
                p->colors[0][i] = p->colors[3][i];
            }
            // B→R
            for (i = 0; i < 3; i++) {
                p->colors[3][i] = p->colors[1][i];
            }
            // L→B
            for (i = 0; i < 3; i++) {
                p->colors[1][i] = p->colors[2][i];
            }
            // F→L
            for (i = 0; i < 3; i++) {
                p->colors[2][i] = temp[i];
            }
        }
    }
}

// Rotate colors of pieces on a face
// Note: Colors are rotated along with positions, so this function is now redundant
// but kept for future color-only rotations if needed
void rotateColors(int face, bool clockwise) {
    // Colors are already rotated in rotatePositions() via memcpy of entire CubePiece
    // This function is kept for compatibility but does nothing
    (void)face;
    (void)clockwise;
}

float easeInOutCubic(float t) {
    if (t < 0.0f) {
        t = 0.0f;
    } else if (t > 1.0f) {
        t = 1.0f;
    }
    if (t < 0.5f) {
        return 4.0f * t * t * t;
    }
    float f = (2.0f * t) - 2.0f;
    return 0.5f * f * f * f + 1.0f;
}

bool isPieceInAnimation(int pieceIndex) {
    if (!g_animation.isActive) {
        return false;
    }
    for (int i = 0; i < 9; i++) {
        if (g_animation.affectedIndices[i] == pieceIndex) {
            return true;
        }
    }
    return false;
}

void cancelAnimationAndQueue() {
    g_animation.isActive = false;
    g_animation.isScrambleMove = false;
    g_animation.currentAngle = 0.0f;
    g_animation.displayAngle = 0.0f;
    for (int i = 0; i < 9; i++) {
        g_animation.affectedIndices[i] = -1;
    }
    g_moveQueue.count = 0;
    g_moveQueue.head = 0;
    for (int i = 0; i < MOVE_QUEUE_CAPACITY; i++) {
        g_moveQueue.scrambleFlags[i] = false;
    }
}

bool dequeueQueuedMove(Face& face, bool& clockwise, bool& isScrambleMove) {
    if (g_moveQueue.count == 0) {
        return false;
    }
    int idx = g_moveQueue.head;
    face = static_cast<Face>(g_moveQueue.moves[idx]);
    clockwise = g_moveQueue.dirs[idx];
    isScrambleMove = g_moveQueue.scrambleFlags[idx];
    g_moveQueue.scrambleFlags[idx] = false;
    g_moveQueue.head = (g_moveQueue.head + 1) % MOVE_QUEUE_CAPACITY;
    g_moveQueue.count--;
    if (g_moveQueue.count == 0) {
        g_moveQueue.head = 0;
    }
    return true;
}

void startRotation(Face face, bool clockwise, bool isScrambleMove) {
    if (face < FRONT || face > DOWN) {
        return;
    }
    if (g_animation.isActive) {
        if (g_moveQueue.count >= MOVE_QUEUE_CAPACITY) {
            if (g_logFile != NULL) {
                const char* faceNames[] = {"FRONT", "BACK", "LEFT", "RIGHT", "UP", "DOWN"};
                double tsMs = getLogTimestampMs();
                fprintf(g_logFile, "[%010.3f ms] QUEUE FULL: drop %s %s\n",
                        tsMs,
                        faceNames[face],
                        clockwise ? "CW" : "CCW");
                fflush(g_logFile);
            }
        } else {
            int idx = (g_moveQueue.head + g_moveQueue.count) % MOVE_QUEUE_CAPACITY;
            g_moveQueue.moves[idx] = static_cast<int>(face);
            g_moveQueue.dirs[idx] = clockwise;
            g_moveQueue.scrambleFlags[idx] = isScrambleMove;
            g_moveQueue.count++;
            if (g_logFile != NULL) {
                const char* faceNames[] = {"FRONT", "BACK", "LEFT", "RIGHT", "UP", "DOWN"};
                double tsMs = getLogTimestampMs();
                fprintf(g_logFile, "[%010.3f ms] ANIM QUEUED %s %s | queue=%d\n",
                        tsMs,
                        faceNames[face],
                        clockwise ? "CW" : "CCW",
                        g_moveQueue.count);
                fflush(g_logFile);
            }
        }
        return;
    }
    onMoveStarted();
    g_animation.isActive = true;
    g_animation.face = face;
    g_animation.clockwise = clockwise;
    g_animation.isScrambleMove = isScrambleMove;
    g_animation.currentAngle = 0.0f;
    g_animation.displayAngle = 0.0f;
    g_animation.targetAngle = 90.0f;
    g_animation.speed = ROTATION_SPEED_DEG_PER_SEC;
    getFaceIndices(face, g_animation.affectedIndices);
    if (g_logFile != NULL) {
        const char* faceNames[] = {"FRONT", "BACK", "LEFT", "RIGHT", "UP", "DOWN"};
        double tsMs = getLogTimestampMs();
        fprintf(g_logFile, "[%010.3f ms] ANIM START %s %s | queue=%d\n",
                tsMs,
                faceNames[face],
                clockwise ? "CW" : "CCW",
                g_moveQueue.count);
        fflush(g_logFile);
    }
    glutPostRedisplay();
}

void updateAnimation(float deltaTime) {
    if (!g_animation.isActive) {
        return;
    }
    g_animation.currentAngle += g_animation.speed * deltaTime;
    if (g_animation.currentAngle > g_animation.targetAngle) {
        g_animation.currentAngle = g_animation.targetAngle;
    }
    float progress = (g_animation.targetAngle > 0.0f)
        ? (g_animation.currentAngle / g_animation.targetAngle)
        : 1.0f;
    if (progress > 1.0f) {
        progress = 1.0f;
    }
    g_animation.displayAngle = easeInOutCubic(progress) * g_animation.targetAngle;
    if (g_animation.currentAngle >= g_animation.targetAngle - 0.0001f) {
        Face finishedFace = g_animation.face;
        bool finishedDir = g_animation.clockwise;
        bool finishedWasScramble = g_animation.isScrambleMove;
        rotateFace(finishedFace, finishedDir);
        g_animation.isActive = false;
        g_animation.isScrambleMove = false;
        g_animation.currentAngle = 0.0f;
        g_animation.displayAngle = 0.0f;
        for (int i = 0; i < 9; i++) {
            g_animation.affectedIndices[i] = -1;
        }
        if (g_logFile != NULL) {
            const char* faceNames[] = {"FRONT", "BACK", "LEFT", "RIGHT", "UP", "DOWN"};
            double tsMs = getLogTimestampMs();
            fprintf(g_logFile, "[%010.3f ms] ANIM END %s %s | queue=%d\n",
                    tsMs,
                    faceNames[finishedFace],
                    finishedDir ? "CW" : "CCW",
                    g_moveQueue.count);
            fflush(g_logFile);
        }
        handleScrambleMoveCompletion(finishedWasScramble);
        Face nextFace;
        bool nextDir;
        bool nextIsScramble = false;
        if (dequeueQueuedMove(nextFace, nextDir, nextIsScramble)) {
            startRotation(nextFace, nextDir, nextIsScramble);
        }
    }
    glutPostRedisplay();
}

void onMoveStarted() {
    if (g_timer.state == TIMER_READY) {
        g_timer.state = TIMER_RUNNING;
        g_timer.startTime = (float)glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
        g_timer.lastSampleTime = g_timer.startTime;
        g_timer.moveCount = 0;
        g_timer.currentTime = 0.0f;
        g_timer.tps = 0.0f;
        if (g_logFile != NULL) {
            fprintf(g_logFile, "TIMER STARTED\n");
            fflush(g_logFile);
        }
    }
    if (g_timer.state == TIMER_RUNNING) {
        g_timer.moveCount++;
    }
}

bool isCubeSolved() {
    const float tolerance = 0.05f;
    int indices[9];
    for (int face = 0; face < 6; face++) {
        getFaceIndices(face, indices);
        const float* centerColor = g_rubikCube.pieces[indices[4]].colors[face];
        for (int i = 0; i < 9; i++) {
            const float* sticker = g_rubikCube.pieces[indices[i]].colors[face];
            float diff = fabs(sticker[0] - centerColor[0]) +
                        fabs(sticker[1] - centerColor[1]) +
                        fabs(sticker[2] - centerColor[2]);
            if (diff > tolerance) {
                return false;
            }
        }
    }
    return true;
}

void updateTimer() {
    if (g_timer.state != TIMER_RUNNING) {
        return;
    }
    float now = (float)glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    g_timer.currentTime = now - g_timer.startTime;
    g_timer.lastSampleTime = now;
    if (g_timer.currentTime < 0.0f) {
        g_timer.currentTime = 0.0f;
    }
    if (g_timer.currentTime > 0.0f) {
        g_timer.tps = (float)g_timer.moveCount / g_timer.currentTime;
    }
    if (!g_animation.isActive && isCubeSolved()) {
        g_timer.state = TIMER_STOPPED;
        g_timer.endTime = g_timer.currentTime;
        if (g_logFile != NULL) {
            fprintf(g_logFile, "========================================\n");
            fprintf(g_logFile, "CUBE SOLVED!\n");
            fprintf(g_logFile, "Time: %.2f seconds\n", g_timer.endTime);
            fprintf(g_logFile, "Moves: %d\n", g_timer.moveCount);
            fprintf(g_logFile, "TPS: %.2f\n", g_timer.tps);
            fprintf(g_logFile, "========================================\n");
            fflush(g_logFile);
        }
    }
    glutPostRedisplay();
}

void idle() {
    int currentTime = glutGet(GLUT_ELAPSED_TIME);
    if (g_lastTimeMs == 0) {
        g_lastTimeMs = currentTime;
    }
    float deltaTime = (float)(currentTime - g_lastTimeMs) / 1000.0f;
    if (deltaTime > 0.1f) {
        deltaTime = 0.1f;
    } else if (deltaTime < 0.0f) {
        deltaTime = 0.0f;
    }
    g_lastTimeMs = currentTime;
    updateAnimation(deltaTime);
    updateTimer();
}

bool performRelativeFaceTurn(int relativeFace, bool clockwise) {
    Face absoluteFace = getAbsoluteFace(relativeFace);
    startRotation(absoluteFace, clockwise);
    return true;
}

// Convert relative face (F/U/R/L/D/B) to absolute face based on current front face
// Relative faces: F=Front, U=Up, R=Right, L=Left, D=Down, B=Back (relative to current view)
Face getAbsoluteFace(int relativeFace) {
    ViewFaceMapping mapping;
    computeViewFaceMapping(mapping);
    Face selected = mapping.front;
    switch (relativeFace) {
        case 0: selected = mapping.front; break;
        case 1: selected = mapping.up; break;
        case 2: selected = mapping.right; break;
        case 3: selected = mapping.left; break;
        case 4: selected = mapping.down; break;
        case 5: selected = mapping.back; break;
        default: selected = mapping.front; break;
    }
    if (g_logFile != NULL) {
        const char* faceNames[] = {"FRONT", "BACK", "LEFT", "RIGHT", "UP", "DOWN"};
        double tsMs = getLogTimestampMs();
        fprintf(g_logFile,
            "[%010.3f ms] REL FACE %d -> %s (front=%s up=%s right=%s) angles(X=%.1f,Y=%.1f)\n",
            tsMs,
            relativeFace,
            faceNames[selected],
            faceNames[mapping.front],
            faceNames[mapping.up],
            faceNames[mapping.right],
            cameraAngleX,
            cameraAngleY);
        fflush(g_logFile);
    }
    return selected;
}

// Main face rotation function
void rotateFace(int face, bool clockwise) {
    int indices[9];
    getFaceIndices(face, indices);
    
    // Rotate positions (this also rotates colors since we swap entire CubePiece)
    rotatePositions(face, clockwise);
    
    // Log rotation with piece details
    if (g_logFile != NULL) {
        const char* faceNames[] = {"FRONT", "BACK", "LEFT", "RIGHT", "UP", "DOWN"};
        double tsMs = getLogTimestampMs();
        fprintf(g_logFile, "[%010.3f ms] ROTATE %s %s: pieces [%d,%d,%d,%d,%d,%d,%d,%d,%d]\n", 
            tsMs,
            faceNames[face], clockwise ? "CW" : "CCW",
            indices[0], indices[1], indices[2], indices[3], indices[4],
            indices[5], indices[6], indices[7], indices[8]);
        
        // Log first piece colors after rotation
        CubePiece& p = g_rubikCube.pieces[indices[0]];
        fprintf(g_logFile, "[%010.3f ms] Piece %d AFTER: F=[%.1f,%.1f,%.1f] B=[%.1f,%.1f,%.1f] L=[%.1f,%.1f,%.1f] R=[%.1f,%.1f,%.1f] U=[%.1f,%.1f,%.1f] D=[%.1f,%.1f,%.1f]\n",
            tsMs,
                indices[0],
                p.colors[0][0], p.colors[0][1], p.colors[0][2],
                p.colors[1][0], p.colors[1][1], p.colors[1][2],
                p.colors[2][0], p.colors[2][1], p.colors[2][2],
                p.colors[3][0], p.colors[3][1], p.colors[3][2],
                p.colors[4][0], p.colors[4][1], p.colors[4][2],
                p.colors[5][0], p.colors[5][1], p.colors[5][2]);
        fflush(g_logFile);
    }
}

// Reset cube to solved state
void resetCube() {
    cancelAnimationAndQueue();
    initRubikCube();
    g_scrambleMovesPending = 0;
    resetTimerState();
    if (g_logFile != NULL) {
        fprintf(g_logFile, "RESET: Cube to solved state\n");
        fflush(g_logFile);
    }
}

// Shuffle cube with random moves
void shuffleCube(int numMoves) {
    if (numMoves <= 0) {
        return;
    }
    resetTimerState();
    g_scrambleMovesPending = numMoves;
    for (int i = 0; i < numMoves; i++) {
        Face face = static_cast<Face>(rand() % 6);
        bool clockwise = (rand() % 2) == 0;
        startRotation(face, clockwise, true);
    }
    if (g_logFile != NULL) {
        fprintf(g_logFile, "SHUFFLE: %d random moves queued\n", numMoves);
        fflush(g_logFile);
    }
}

// Initialize OpenGL settings
void initOpenGL() {
    // Disable face culling to show all 6 faces of each piece
    glDisable(GL_CULL_FACE);
    
    // Enable depth testing for 3D rendering
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Set clear color to dark gray
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    
    // Enable smooth shading
    glShadeModel(GL_SMOOTH);
    
    // Disable OpenGL lighting so cube colors stay flat and matching textures
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    glDisable(GL_COLOR_MATERIAL);
}

// Draw a single cube piece with 6 custom colors
void drawCubePiece(const CubePiece& piece) {
    const float size = g_rubikCube.pieceSize * 0.5f; // Half size
    
    glBegin(GL_QUADS);
    
    // Front face (Z+) - Face 0
    glColor3fv(piece.colors[0]);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-size, -size, size);
    glVertex3f(size, -size, size);
    glVertex3f(size, size, size);
    glVertex3f(-size, size, size);
    
    // Back face (Z-) - Face 1
    glColor3fv(piece.colors[1]);
    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(size, -size, -size);
    glVertex3f(-size, -size, -size);
    glVertex3f(-size, size, -size);
    glVertex3f(size, size, -size);
    
    // Left face (X-) - Face 2
    glColor3fv(piece.colors[2]);
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(-size, -size, -size);
    glVertex3f(-size, -size, size);
    glVertex3f(-size, size, size);
    glVertex3f(-size, size, -size);
    
    // Right face (X+) - Face 3
    glColor3fv(piece.colors[3]);
    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(size, -size, size);
    glVertex3f(size, -size, -size);
    glVertex3f(size, size, -size);
    glVertex3f(size, size, size);
    
    // Up face (Y+) - Face 4
    glColor3fv(piece.colors[4]);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-size, size, size);
    glVertex3f(size, size, size);
    glVertex3f(size, size, -size);
    glVertex3f(-size, size, -size);
    
    // Down face (Y-) - Face 5
    glColor3fv(piece.colors[5]);
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(-size, -size, -size);
    glVertex3f(size, -size, -size);
    glVertex3f(size, -size, size);
    glVertex3f(-size, -size, size);
    
    glEnd();
}

// Initialize Rubik's Cube with 27 pieces and standard colors
void initRubikCube() {
    // Set cube properties
    g_rubikCube.pieceSize = PIECE_SIZE;
    g_rubikCube.gapSize = GAP_SIZE;
    
    // Initialize all 27 pieces
    // Loop order: Front to Back (k: +1 to -1) so Piece 0 has Front face
    int index = 0;
    for (int k = 1; k >= -1; k--) {      // Z: Front to Back (k: +1, 0, -1)
        for (int j = -1; j <= 1; j++) {  // Y: Down to Up
            for (int i = -1; i <= 1; i++) { // X: Left to Right
                CubePiece& piece = g_rubikCube.pieces[index];
                
                // Set position
                piece.position[0] = i;
                piece.position[1] = j;
                piece.position[2] = k;
                
                // All pieces visible in Phase 2
                piece.isVisible = true;
                
                // Initialize all faces to black (hidden) first
                for (int face = 0; face < 6; face++) {
                    piece.colors[face][0] = COLOR_BLACK[0];
                    piece.colors[face][1] = COLOR_BLACK[1];
                    piece.colors[face][2] = COLOR_BLACK[2];
                }
                
                // Assign colors based on position (Standard Rubik's Cube colors)
                // Face indices: 0=Front, 1=Back, 2=Left, 3=Right, 4=Up, 5=Down
                
                // Front face (Z+ = +1) - RED
                if (k == 1) {
                    piece.colors[0][0] = COLOR_RED[0];
                    piece.colors[0][1] = COLOR_RED[1];
                    piece.colors[0][2] = COLOR_RED[2];
                }
                
                // Back face (Z- = -1) - ORANGE
                if (k == -1) {
                    piece.colors[1][0] = COLOR_ORANGE[0];
                    piece.colors[1][1] = COLOR_ORANGE[1];
                    piece.colors[1][2] = COLOR_ORANGE[2];
                }
                
                // Left face (X- = -1) - BLUE
                if (i == -1) {
                    piece.colors[2][0] = COLOR_BLUE[0];
                    piece.colors[2][1] = COLOR_BLUE[1];
                    piece.colors[2][2] = COLOR_BLUE[2];
                }
                
                // Right face (X+ = +1) - GREEN
                if (i == 1) {
                    piece.colors[3][0] = COLOR_GREEN[0];
                    piece.colors[3][1] = COLOR_GREEN[1];
                    piece.colors[3][2] = COLOR_GREEN[2];
                }
                
                // Up face (Y+ = +1) - WHITE
                if (j == 1) {
                    piece.colors[4][0] = COLOR_WHITE[0];
                    piece.colors[4][1] = COLOR_WHITE[1];
                    piece.colors[4][2] = COLOR_WHITE[2];
                }
                
                // Down face (Y- = -1) - YELLOW
                if (j == -1) {
                    piece.colors[5][0] = COLOR_YELLOW[0];
                    piece.colors[5][1] = COLOR_YELLOW[1];
                    piece.colors[5][2] = COLOR_YELLOW[2];
                }
                
                index++;
            }
        }
    }
    
    // Log initialization - Piece 0 now has Front face (k=+1)
    if (g_logFile != NULL) {
        fprintf(g_logFile, "Phase 2: Initialized %d Rubik pieces\n", 27);
        fprintf(g_logFile, "Piece 0: F=[%.1f,%.1f,%.1f] B=[%.1f,%.1f,%.1f] L=[%.1f,%.1f,%.1f] R=[%.1f,%.1f,%.1f] U=[%.1f,%.1f,%.1f] D=[%.1f,%.1f,%.1f]\n",
                g_rubikCube.pieces[0].colors[0][0], g_rubikCube.pieces[0].colors[0][1], g_rubikCube.pieces[0].colors[0][2],
                g_rubikCube.pieces[0].colors[1][0], g_rubikCube.pieces[0].colors[1][1], g_rubikCube.pieces[0].colors[1][2],
                g_rubikCube.pieces[0].colors[2][0], g_rubikCube.pieces[0].colors[2][1], g_rubikCube.pieces[0].colors[2][2],
                g_rubikCube.pieces[0].colors[3][0], g_rubikCube.pieces[0].colors[3][1], g_rubikCube.pieces[0].colors[3][2],
                g_rubikCube.pieces[0].colors[4][0], g_rubikCube.pieces[0].colors[4][1], g_rubikCube.pieces[0].colors[4][2],
                g_rubikCube.pieces[0].colors[5][0], g_rubikCube.pieces[0].colors[5][1], g_rubikCube.pieces[0].colors[5][2]);
        fflush(g_logFile);
    }
}

// Draw the complete 3x3x3 Rubik's Cube
void drawRubikCube() {
    glPushMatrix();
    
    // Draw all 27 pieces
    for (int i = 0; i < 27; i++) {
        const CubePiece& piece = g_rubikCube.pieces[i];
        
        if (!piece.isVisible) {
            continue;
        }
        
        // Calculate world position based on grid position
        // Position = grid_pos * (pieceSize + gapSize)
        float worldX = (float)piece.position[0] * (g_rubikCube.pieceSize + g_rubikCube.gapSize);
        float worldY = (float)piece.position[1] * (g_rubikCube.pieceSize + g_rubikCube.gapSize);
        float worldZ = (float)piece.position[2] * (g_rubikCube.pieceSize + g_rubikCube.gapSize);
        
        // Save current matrix
        glPushMatrix();
        
        bool pieceAnimating = g_animation.isActive && isPieceInAnimation(i);
        if (pieceAnimating) {
            float axisX = 0.0f;
            float axisY = 0.0f;
            float axisZ = 0.0f;
            int axisSign = 1;
            switch (g_animation.face) {
                case FRONT:
                    axisZ = 1.0f;
                    axisSign = 1;
                    break;
                case BACK:
                    axisZ = 1.0f;
                    axisSign = -1;
                    break;
                case LEFT:
                    axisX = 1.0f;
                    axisSign = -1;
                    break;
                case RIGHT:
                    axisX = 1.0f;
                    axisSign = 1;
                    break;
                case UP:
                    axisY = 1.0f;
                    axisSign = 1;
                    break;
                case DOWN:
                    axisY = 1.0f;
                    axisSign = -1;
                    break;
            }
            float angle = g_animation.clockwise ? -g_animation.displayAngle : g_animation.displayAngle;
            angle *= static_cast<float>(axisSign);
            glRotatef(angle, axisX, axisY, axisZ);
        }
        
        // Translate to piece position after optional face rotation so the whole layer pivots together
        glTranslatef(worldX, worldY, worldZ);
        
        // Draw the piece with its colors
        drawCubePiece(piece);
        
        // Restore matrix
        glPopMatrix();
    }
    
    glPopMatrix();
}

void renderBitmapString(float x, float y, void* font, const char* string) {
    glRasterPos2f(x, y);
    while (*string != '\0') {
        glutBitmapCharacter(font, *string);
        ++string;
    }
}

void formatTimerText(float seconds, char* buffer, int bufferSize) {
    if (seconds < 0.0f) {
        seconds = 0.0f;
    }
    int minutes = (int)(seconds / 60.0f);
    float sec = seconds - minutes * 60.0f;
    if (bufferSize > 0) {
        snprintf(buffer, bufferSize, "%02d:%06.3f", minutes, sec);
    }
}

void displayTimerOverlay() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0.0f, (float)windowWidth, 0.0f, (float)windowHeight);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean lightingEnabled = glIsEnabled(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    char buffer[128];
    if (g_scrambleMovesPending > 0) {
        glColor3f(1.0f, 0.6f, 0.0f);
        snprintf(buffer, sizeof(buffer), "Scrambling... (%d moves left)", g_scrambleMovesPending);
        renderBitmapString(10.0f, windowHeight - 20.0f, GLUT_BITMAP_HELVETICA_18, buffer);
        renderBitmapString(10.0f, windowHeight - 40.0f, GLUT_BITMAP_HELVETICA_18,
                           "Please wait for scramble to finish");
    } else {
        switch (g_timer.state) {
            case TIMER_IDLE:
                glColor3f(1.0f, 1.0f, 1.0f);
                renderBitmapString(10.0f, windowHeight - 20.0f, GLUT_BITMAP_HELVETICA_18,
                                   "Press 'S' to scramble");
                break;
            case TIMER_READY:
                glColor3f(1.0f, 1.0f, 0.2f);
                renderBitmapString(10.0f, windowHeight - 20.0f, GLUT_BITMAP_HELVETICA_18,
                                   "READY - Make first move to start");
                break;
            case TIMER_RUNNING:
                glColor3f(0.0f, 1.0f, 0.0f);
                formatTimerText(g_timer.currentTime, buffer, sizeof(buffer));
                renderBitmapString(10.0f, windowHeight - 20.0f, GLUT_BITMAP_HELVETICA_18,
                                   buffer);
                snprintf(buffer, sizeof(buffer), "Moves: %d", g_timer.moveCount);
                renderBitmapString(10.0f, windowHeight - 40.0f, GLUT_BITMAP_HELVETICA_18,
                                   buffer);
                snprintf(buffer, sizeof(buffer), "TPS: %.2f", g_timer.tps);
                renderBitmapString(10.0f, windowHeight - 60.0f, GLUT_BITMAP_HELVETICA_18,
                                   buffer);
                break;
            case TIMER_STOPPED:
                glColor3f(0.2f, 1.0f, 0.2f);
                snprintf(buffer, sizeof(buffer), "Solved! Time %.2fs | Moves %d | TPS %.2f",
                         g_timer.endTime, g_timer.moveCount, g_timer.tps);
                renderBitmapString(windowWidth * 0.2f, windowHeight * 0.5f,
                                   GLUT_BITMAP_HELVETICA_18, buffer);
                break;
        }
    }
    if (depthEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
    if (lightingEnabled) {
        glEnable(GL_LIGHTING);
    }
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// Main display function - renders the scene
void display() {
    // Clear color and depth buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Reset modelview matrix
    glLoadIdentity();
    
    // DEBUG: Log frame count periodically (every 30 frames to avoid spam)
    static int frameCount = 0;
    if (frameCount++ % 30 == 0 && g_logFile != NULL) {
        fprintf(g_logFile, "DISPLAY: frame=%d\n", frameCount);
        fflush(g_logFile);
    }
    
    // Apply camera transformations
    // Move camera back from origin first
    glTranslatef(0.0f, 0.0f, -CAMERA_DISTANCE);
    
    // Apply accumulated rotation using dynamic axes
    // Apply horizontal rotation first, then vertical rotation
    rotateAroundAxis(horizontalAxis, cameraAngleY);
    rotateAroundAxis(verticalAxis, cameraAngleX);
    
    // Draw the complete 3x3x3 Rubik's Cube
    drawRubikCube();
    displayTimerOverlay();
    
    // Swap buffers to display the rendered frame
    glutSwapBuffers();
}

// Handle window reshape events
void reshape(int w, int h) {
    // Update global window dimensions
    windowWidth = w;
    windowHeight = h;
    
    // Prevent division by zero
    if (h == 0) {
        h = 1;
    }
    
    // Set viewport to entire window
    glViewport(0, 0, w, h);
    
    // Switch to projection matrix mode
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    // Set up perspective projection
    // FOV = 45 degrees, aspect ratio, near = 0.1, far = 100.0
    const float fov = 45.0f;
    const float aspect = (float)w / (float)h;
    const float nearPlane = 0.1f;
    const float farPlane = 100.0f;
    
    gluPerspective(fov, aspect, nearPlane, farPlane);
    
    // Switch back to modelview matrix mode
    glMatrixMode(GL_MODELVIEW);
}

// Handle mouse button press/release events
void mouse(int button, int state, int x, int y) {
    // DEBUG: Log all mouse button events to file
    if (g_logFile != NULL) {
        fprintf(g_logFile, "MOUSE EVENT: button=%d state=%d x=%d y=%d\n", button, state, x, y);
        fflush(g_logFile);
    }
    
    // Only handle left mouse button
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            // Start dragging: save initial mouse position
            if (g_logFile != NULL) {
                fprintf(g_logFile, "*** DRAG START ***\n");
                fflush(g_logFile);
            }
            isDragging = true;
            lastMouseX = x;
            lastMouseY = y;
        } else if (state == GLUT_UP) {
            // Stop dragging
            if (g_logFile != NULL) {
                fprintf(g_logFile, "*** DRAG END ***\n");
                fflush(g_logFile);
            }
            isDragging = false;
        }
    }
    glutPostRedisplay();
}

// Handle mouse motion while button is pressed (dragging)
void motion(int x, int y) {
    // Only process motion if we're currently dragging
    if (!isDragging) {
        return;
    }
    
    // Calculate mouse movement delta
    int dx = x - lastMouseX;
    int dy = y - lastMouseY;
    
    // STANDARD ARC-BALL MATHEMATICS:
    // Mouse coordinate: Y=0 at top, increases DOWNWARD
    // OpenGL world: Y+ points UPWARD
    // 
    // For intuitive control:
    // - Mouse RIGHT (dx > 0) → Yaw RIGHT (positive)
    // - Mouse LEFT (dx < 0) → Yaw LEFT (negative)
    // - Mouse UP (y decreases) → Camera looks UP (pitch DECREASES, negative)
    // - Mouse DOWN (y increases) → Camera looks DOWN (pitch INCREASES, positive)
    //
    // Solution: Use (y - lastMouseY) directly for pitchDelta
    // When mouse UP: y decreases → (y - lastMouseY) < 0 → pitchDelta negative → angleX decreases → look UP ✓
    // When mouse DOWN: y increases → (y - lastMouseY) > 0 → pitchDelta positive → angleX increases → look DOWN ✓
    
    float yawDelta = (float)dx * ROTATION_SENSITIVITY;           // RIGHT = positive yaw
    float pitchDelta = (float)(y - lastMouseY) * ROTATION_SENSITIVITY;  // FLIP Y LOGIC: UP = negative pitch (look up)
    
    // DEBUG: Log mouse movement and calculated deltas to file
    if (g_logFile != NULL) {
        fprintf(g_logFile, "MOUSE: x=%d y=%d | dx=%d dy=%d | yawDelta=%.1f pitchDelta=%.1f\n", 
                x, y, dx, dy, yawDelta, pitchDelta);
        fflush(g_logFile);
    }
    
    // Apply rotation deltas (accumulate angles)
    cameraAngleY += yawDelta;   // Yaw: horizontal rotation (left/right)
    cameraAngleX += pitchDelta; // Pitch: vertical rotation (up/down)
    
    // Allow unlimited rotation - no clamping
    // User can rotate as much as they want in any direction
    // Note: This allows full 360° rotation in all directions
    
    // DEBUG: Log angles after update to file
    if (g_logFile != NULL) {
        fprintf(g_logFile, "  → Updated angles: X=%.1f Y=%.1f\n", cameraAngleX, cameraAngleY);
        fflush(g_logFile);
    }
    
    // Update last mouse position for next frame
    lastMouseX = x;
    lastMouseY = y;
    
    // Request redraw to update camera view
    glutPostRedisplay();
}

// Handle regular keyboard input (F/R/B/L/U/D for face selection)
void keyboard(unsigned char key, int /* x */, int /* y */) {
    int modifiers = glutGetModifiers();
    bool shiftDown = (modifiers & GLUT_ACTIVE_SHIFT) != 0;
    int keyUpper = toupper((unsigned char)key);
    bool trackKey = false;
    switch (keyUpper) {
        case 'F':
        case 'U':
        case 'R':
        case 'L':
        case 'D':
        case 'B':
        case 'S':
            trackKey = true;
            break;
        default:
            break;
    }
    if (key == ' ') {
        trackKey = true;
        keyUpper = ' ';
    }
    if (trackKey) {
        unsigned char idx = (unsigned char)keyUpper;
        if (g_keyHeld[idx]) {
            return;
        }
        g_keyHeld[idx] = true;
    }
    Face newFace = currentFrontFace;
    bool faceChanged = false;
    
    switch (keyUpper) {
        case ' ': // Spacebar - Reset cube to solved state
            resetCube();
            glutPostRedisplay();
            return;
            
        case 'S': // Shuffle cube
            shuffleCube(20);
            glutPostRedisplay();
            return;
            
        // Face rotation controls (relative to current front face)
        // F/U/R/L/D/B = Front/Up/Right/Left/Down/Back relative to current view
        case 'F': // Front face (relative)
            performRelativeFaceTurn(0, !shiftDown);  // Shift => CCW
            return;
            
        case 'U': // Up face (relative)
            performRelativeFaceTurn(1, !shiftDown);
            return;
            
        case 'R': // Right face (relative)
            performRelativeFaceTurn(2, !shiftDown);
            return;
            
        case 'L': // Left face (relative)
            performRelativeFaceTurn(3, !shiftDown);
            return;
            
        case 'D': // Down face (relative)
            performRelativeFaceTurn(4, !shiftDown);
            return;
            
        case 'B': // Back face (relative)
            performRelativeFaceTurn(5, !shiftDown);
            return;
            
        // Camera face selection (lowercase)
        case 'f':
            newFace = FRONT;
            faceChanged = true;
            break;
            
        case 'r':
            newFace = RIGHT;
            faceChanged = true;
            break;
            
        case 'b':
            newFace = BACK;
            faceChanged = true;
            break;
            
        case 'l':
            newFace = LEFT;
            faceChanged = true;
            break;
            
        case 'u':
            newFace = UP;
            faceChanged = true;
            break;
            
        case 'd':
            newFace = DOWN;
            faceChanged = true;
            break;
            
        default:
            // Ignore other keys
            break;
    }
    
    switch (key) {
        case 'f':
            newFace = FRONT;
            faceChanged = true;
            break;
            
        case 'r':
            newFace = RIGHT;
            faceChanged = true;
            break;
            
        case 'b':
            newFace = BACK;
            faceChanged = true;
            break;
            
        case 'l':
            newFace = LEFT;
            faceChanged = true;
            break;
            
        case 'u':
            newFace = UP;
            faceChanged = true;
            break;
            
        case 'd':
            newFace = DOWN;
            faceChanged = true;
            break;
        default:
            break;
    }
    
    if (faceChanged) {
        currentFrontFace = newFace;
        updateRotationAxes();
        glutPostRedisplay();
    }
}

void keyboardUp(unsigned char key, int /* x */, int /* y */) {
    int keyUpper = toupper((unsigned char)key);
    if (keyUpper < 0 || keyUpper >= 256) {
        return;
    }
    g_keyHeld[(unsigned char)keyUpper] = false;
    if (key == ' ') {
        g_keyHeld[(unsigned char)' '] = false;
    }
}

// Handle special keyboard input (arrow keys for rotation around dynamic axes)
void keyboardSpecial(int key, int /* x */, int /* y */) {
    const float ROTATION_STEP = KEYBOARD_ROTATION_SPEED;
    const char* keyName = "";
    
    switch (key) {
        case GLUT_KEY_UP:
            // Rotate around vertical axis (negative for up)
            cameraAngleX -= ROTATION_STEP;
            keyName = "UP";
            break;
            
        case GLUT_KEY_DOWN:
            // Rotate around vertical axis (positive for down)
            cameraAngleX += ROTATION_STEP;
            keyName = "DOWN";
            break;
            
        case GLUT_KEY_LEFT:
            // Rotate around horizontal axis (negative for left)
            cameraAngleY -= ROTATION_STEP;
            keyName = "LEFT";
            break;
            
        case GLUT_KEY_RIGHT:
            // Rotate around horizontal axis (positive for right)
            cameraAngleY += ROTATION_STEP;
            keyName = "RIGHT";
            break;
            
        default:
            // Ignore other keys
            return;
    }
    
    // Log rotation
    if (g_logFile != NULL) {
        fprintf(g_logFile, "KEYBOARD: %s pressed | angleX=%.1f angleY=%.1f\n", keyName, cameraAngleX, cameraAngleY);
        fflush(g_logFile);
    }
    
    // Request redraw to update camera view
    glutPostRedisplay();
}

// Test function: Verify face^4 = identity for all faces (4 CW turns return to original state)
void testRotationIdentity() {
    if (g_logFile == NULL) {
        return;
    }
    
    fprintf(g_logFile, "\n=== ROTATION IDENTITY TEST ===\n");
    
    // Backup entire cube colors
    float originalColors[27][6][3];
    for (int p = 0; p < 27; p++) {
        for (int f = 0; f < 6; f++) {
            for (int c = 0; c < 3; c++) {
                originalColors[p][f][c] = g_rubikCube.pieces[p].colors[f][c];
            }
        }
    }
    
    const Face facesToTest[] = {FRONT, BACK, LEFT, RIGHT, UP, DOWN};
    const char* faceNames[] = {"FRONT", "BACK", "LEFT", "RIGHT", "UP", "DOWN"};
    const int entriesPerCube = 27 * 6 * 3;
    
    for (int faceIdx = 0; faceIdx < 6; faceIdx++) {
        Face face = facesToTest[faceIdx];
        // Restore baseline before each face test
        for (int p = 0; p < 27; p++) {
            for (int f = 0; f < 6; f++) {
                for (int c = 0; c < 3; c++) {
                    g_rubikCube.pieces[p].colors[f][c] = originalColors[p][f][c];
                }
            }
        }
        
        fprintf(g_logFile, "Testing %s: performing 4 CW turns...\n", faceNames[faceIdx]);
        for (int turn = 0; turn < 4; turn++) {
            rotateFace(face, true);
        }
        
        int matches = 0;
        for (int p = 0; p < 27; p++) {
            for (int f = 0; f < 6; f++) {
                for (int c = 0; c < 3; c++) {
                    float diff = fabs(g_rubikCube.pieces[p].colors[f][c] - originalColors[p][f][c]);
                    if (diff < 0.001f) {
                        matches++;
                    }
                }
            }
        }
        
        if (matches == entriesPerCube) {
            fprintf(g_logFile, "  -> %s PASSED (%d/%d matches)\n", faceNames[faceIdx], matches, entriesPerCube);
        } else {
            fprintf(g_logFile, "  -> %s FAILED (%d/%d matches)\n", faceNames[faceIdx], matches, entriesPerCube);
        }
    }
    
    // Restore original solved state after tests
    for (int p = 0; p < 27; p++) {
        for (int f = 0; f < 6; f++) {
            for (int c = 0; c < 3; c++) {
                g_rubikCube.pieces[p].colors[f][c] = originalColors[p][f][c];
            }
        }
    }
    
    fprintf(g_logFile, "=== END ROTATION IDENTITY TEST ===\n\n");
    fflush(g_logFile);
}

// Main entry point
int main(int argc, char** argv) {
    // Initialize debug log file
    initLogFile();
    
    // Initialize GLUT
    glutInit(&argc, argv);
    
    // Set display mode: double buffer + RGB color + depth buffer
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    
    // Set window size and position
    glutInitWindowSize(windowWidth, windowHeight);
    glutInitWindowPosition(100, 100);
    
    // Create window with title
    glutCreateWindow("Rubik's Cube - Phase 2");
    
    // Initialize OpenGL settings
    initOpenGL();
    
    // Initialize Rubik's Cube (27 pieces)
    initRubikCube();
    
    // Test rotation identity: F^4 = identity (verify fix works correctly)
    testRotationIdentity();
    
    // Initialize rotation axes for default FRONT face
    updateRotationAxes();
    
    // Initialize random seed for shuffle
    srand((unsigned int)time(NULL));
    
    // Register callback functions
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutKeyboardFunc(keyboard);      // Register keyboard handler for regular keys (F/R/B/L/U/D)
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(keyboardSpecial); // Register keyboard handler for special keys (arrow keys)
    glutIdleFunc(idle);
    glutIgnoreKeyRepeat(1);           // Disable key auto-repeat so each press logs once
    g_lastTimeMs = glutGet(GLUT_ELAPSED_TIME);
    
    // Log initialization complete
    if (g_logFile != NULL) {
        fprintf(g_logFile, "Application initialized successfully\n\n");
        fflush(g_logFile);
    }
    
    // Enter GLUT main loop (blocks until window is closed)
    glutMainLoop();
    
    // Close log file before exit
    closeLogFile();
    
    // Windows console pause fix - prevents console from closing immediately
#ifdef _WIN32
    system("pause");
#else
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();
#endif
    
	return 0;
}
