/*
 * Rubik's Cube - Phase 1: Basic OpenGL Window with Camera Controls
 * Computer Graphics Final Project
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

// Window dimensions
int windowWidth = 800;
int windowHeight = 600;

// Camera rotation angles (degrees)
float cameraAngleX = 0.0f;
float cameraAngleY = 0.0f;

// Mouse tracking for arcball camera control
bool isDragging = false;
int lastMouseX = 0;
int lastMouseY = 0;

// Debug log file
FILE* g_logFile = NULL;

// Rotation sensitivity (adjust for smooth control)
const float ROTATION_SENSITIVITY = 0.3f;

// Keyboard rotation speed (degrees per key press)
const float KEYBOARD_ROTATION_SPEED = 5.0f;

// Camera distance from origin
const float CAMERA_DISTANCE = 8.0f;

// Initialize log file
void initLogFile() {
    g_logFile = fopen("rubik_debug.log", "w");
    if (g_logFile == NULL) {
        std::cerr << "Warning: Cannot open log file rubik_debug.log" << std::endl;
        return;
    }
    
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

// Initialize OpenGL settings
void initOpenGL() {
    // Enable depth testing for 3D rendering
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Set clear color to dark gray
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    
    // Enable smooth shading
    glShadeModel(GL_SMOOTH);
    
    // Enable lighting (basic setup for better cube visibility)
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    
    // Set up light properties
    GLfloat lightPosition[] = {5.0f, 5.0f, 5.0f, 1.0f};
    GLfloat lightAmbient[] = {0.3f, 0.3f, 0.3f, 1.0f};
    GLfloat lightDiffuse[] = {0.8f, 0.8f, 0.8f, 1.0f};
    
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    
    // Enable color material so glColor3f works with lighting
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
}

// Draw a single cube with 6 different colored faces
void drawCube() {
    const float size = 0.5f; // Half size (total size = 1.0)
    
    glBegin(GL_QUADS);
    
    // Front face (Z+) - RED
    glColor3f(1.0f, 0.0f, 0.0f);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-size, -size, size);
    glVertex3f(size, -size, size);
    glVertex3f(size, size, size);
    glVertex3f(-size, size, size);
    
    // Back face (Z-) - ORANGE
    glColor3f(1.0f, 0.5f, 0.0f);
    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(size, -size, -size);
    glVertex3f(-size, -size, -size);
    glVertex3f(-size, size, -size);
    glVertex3f(size, size, -size);
    
    // Right face (X+) - GREEN
    glColor3f(0.0f, 1.0f, 0.0f);
    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(size, -size, size);
    glVertex3f(size, -size, -size);
    glVertex3f(size, size, -size);
    glVertex3f(size, size, size);
    
    // Left face (X-) - BLUE
    glColor3f(0.0f, 0.0f, 1.0f);
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(-size, -size, -size);
    glVertex3f(-size, -size, size);
    glVertex3f(-size, size, size);
    glVertex3f(-size, size, -size);
    
    // Top face (Y+) - WHITE
    glColor3f(1.0f, 1.0f, 1.0f);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-size, size, size);
    glVertex3f(size, size, size);
    glVertex3f(size, size, -size);
    glVertex3f(-size, size, -size);
    
    // Bottom face (Y-) - YELLOW
    glColor3f(1.0f, 1.0f, 0.0f);
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(-size, -size, -size);
    glVertex3f(size, -size, -size);
    glVertex3f(size, -size, size);
    glVertex3f(-size, -size, size);
    
    glEnd();
}

// Main display function - renders the scene
void display() {
    // Clear color and depth buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Reset modelview matrix
    glLoadIdentity();
    
    // DEBUG: Log camera angles periodically (every 30 frames to avoid spam)
    static int frameCount = 0;
    if (frameCount++ % 30 == 0 && g_logFile != NULL) {
        fprintf(g_logFile, "DISPLAY: frame=%d angleX=%.1f angleY=%.1f\n", frameCount, cameraAngleX, cameraAngleY);
        fflush(g_logFile);
    }
    
    // Apply camera transformations (arcball rotation)
    // Move camera back from origin first
    glTranslatef(0.0f, 0.0f, -CAMERA_DISTANCE);
    
    // CRITICAL: Rotation order matters in OpenGL (applied right-to-left)
    // For intuitive arcball: Apply Yaw FIRST, then Pitch
    // This ensures horizontal rotation is independent of vertical rotation
    glRotatef(cameraAngleY, 0.0f, 1.0f, 0.0f); // Yaw (left/right rotation around Y-axis) - FIRST
    glRotatef(cameraAngleX, 1.0f, 0.0f, 0.0f); // Pitch (up/down rotation around X-axis) - SECOND
    
    // Draw the cube at origin
    drawCube();
    
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
        fprintf(g_logFile, "MOUSE: x=%d y=%d | dx=%d dy=%d | yawDelta=%.1f pitchDelta=%.1f | angles X=%.1f Y=%.1f\n", 
                x, y, dx, dy, yawDelta, pitchDelta, cameraAngleX, cameraAngleY);
        fflush(g_logFile);
    }
    
    // Apply rotation deltas
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

// Handle keyboard input (arrow keys for rotation)
void keyboard(int key, int /* x */, int /* y */) {
    // Handle special keys (arrow keys, function keys, etc.)
    switch (key) {
        case GLUT_KEY_UP:
            // Rotate camera up (decrease pitch angle - look up)
            cameraAngleX -= KEYBOARD_ROTATION_SPEED;
            if (g_logFile != NULL) {
                fprintf(g_logFile, "KEYBOARD: UP pressed | angleX=%.1f angleY=%.1f\n", cameraAngleX, cameraAngleY);
                fflush(g_logFile);
            }
            break;
            
        case GLUT_KEY_DOWN:
            // Rotate camera down (increase pitch angle - look down)
            cameraAngleX += KEYBOARD_ROTATION_SPEED;
            if (g_logFile != NULL) {
                fprintf(g_logFile, "KEYBOARD: DOWN pressed | angleX=%.1f angleY=%.1f\n", cameraAngleX, cameraAngleY);
                fflush(g_logFile);
            }
            break;
            
        case GLUT_KEY_LEFT:
            // Rotate camera left (decrease yaw angle)
            cameraAngleY -= KEYBOARD_ROTATION_SPEED;
            if (g_logFile != NULL) {
                fprintf(g_logFile, "KEYBOARD: LEFT pressed | angleX=%.1f angleY=%.1f\n", cameraAngleX, cameraAngleY);
                fflush(g_logFile);
            }
            break;
            
        case GLUT_KEY_RIGHT:
            // Rotate camera right (increase yaw angle)
            cameraAngleY += KEYBOARD_ROTATION_SPEED;
            if (g_logFile != NULL) {
                fprintf(g_logFile, "KEYBOARD: RIGHT pressed | angleX=%.1f angleY=%.1f\n", cameraAngleX, cameraAngleY);
                fflush(g_logFile);
            }
            break;
            
        default:
            // Ignore other keys
            break;
    }
    
    // Request redraw to update camera view
    glutPostRedisplay();
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
    glutCreateWindow("Rubik's Cube - Phase 1");
    
    // Initialize OpenGL settings
    initOpenGL();
    
    // Register callback functions
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutSpecialFunc(keyboard);  // Register keyboard handler for special keys (arrow keys)
    
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
