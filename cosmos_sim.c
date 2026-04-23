#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <raylib.h>
#include "raymath.h"

// ================= CONFIGURATION =================
#define MAX_PLANETS 8
#define ASTEROID_COUNT 800
#define TRAIL_LENGTH 8
#define MAX_FPS 60

// ================= ORBITAL PARAMETERS =================
typedef struct {
    char name[32];
    double a;
    double e;
    double i;
    double Omega;
    double omega;
    double period;
    float radius;
    float tilt;
    float rot_speed;
    float atmosphereHeight;
    Color atmosphereColor;
    float atmosphereAlpha;
} OrbitalData;

OrbitalData orbitalData[MAX_PLANETS] = {
    {"Mercury", 2.0, 0.2056, 7.0*DEG2RAD, 48.3*DEG2RAD, 29.1*DEG2RAD, 0.2408, 0.38f, 0.01f, 2.0f, 0.02f, (Color){200,180,150,255}, 60},
    {"Venus",   3.0, 0.0068, 3.4*DEG2RAD, 76.7*DEG2RAD, 54.9*DEG2RAD, 0.6152, 0.45f, 177.0f, 0.5f, 0.08f, (Color){255,200,150,255}, 80},
    {"Earth",   4.0, 0.0167, 0.0, -11.26*DEG2RAD, 102.9*DEG2RAD, 1.0, 0.52f, 23.4f, 1.0f, 0.12f, (Color){100,150,255,255}, 100},
    {"Mars",    5.5, 0.0934, 1.85*DEG2RAD, 49.6*DEG2RAD, 336.0*DEG2RAD, 1.8808, 0.45f, 25.0f, 0.9f, 0.06f, (Color){255,150,100,255}, 70},
    {"Jupiter", 9.0, 0.0489, 1.3*DEG2RAD, 100.5*DEG2RAD, 14.3*DEG2RAD, 11.862, 1.15f, 3.0f, 2.0f, 0.20f, (Color){200,170,100,255}, 90},
    {"Saturn",  12.0, 0.0565, 2.5*DEG2RAD, 113.7*DEG2RAD, 92.4*DEG2RAD, 29.457, 1.05f, 26.7f, 2.2f, 0.18f, (Color){210,180,120,255}, 85},
    {"Uranus",  16.0, 0.0472, 0.77*DEG2RAD, 74.0*DEG2RAD, 170.9*DEG2RAD, 84.020, 0.85f, 97.8f, 1.4f, 0.15f, (Color){100,180,200,255}, 75},
    {"Neptune", 19.0, 0.0086, 1.77*DEG2RAD, 131.8*DEG2RAD, 44.9*DEG2RAD, 164.8, 0.85f, 28.3f, 1.3f, 0.15f, (Color){80,100,200,255}, 75}
};

// ================= PLANET STRUCT =================
typedef struct {
    char name[32];
    double a, e, i, Omega, omega, period;
    float radius;
    float tilt;
    float rot_speed;
    Texture2D tex;
    Texture2D cloudTex;
    Model model;
    Model cloudModel;
    Model atmosphereModel;
    Vector3 position;
    Vector3 trail[TRAIL_LENGTH];
    int trailIndex;
    float atmosphereHeight;
    Color atmosphereColor;
    float atmosphereAlpha;
} Planet;

// ================= ASTEROID STRUCT =================
typedef struct {
    Vector3 position;
    float size;
    float speed;
    float angle;
    float radius;
    float yOffset;
    Color color;        // Individual asteroid color
    float rotation;     // Current rotation angle
    float rotSpeed;     // Rotation speed
} Asteroid;

// ================= SELECTION INFO STRUCT =================
typedef struct {
    bool active;
    int selectedType;  // 0=planet, 1=sun, 2=moon
    int selectedIndex;
    float scrollOffset;
    float targetScroll;
} SelectionInfo;

// ================= KEPLER SOLVER =================
static double solve_kepler(double M, double e)
{
    double E = M;
    for (int i = 0; i < 6; i++) {
        double E_new = E - (E - e*sin(E) - M) / (1 - e*cos(E));
        if (fabs(E_new - E) < 1e-10) break;
        E = E_new;
    }
    return E;
}

// ================= ORBIT CALCULATION =================
static Vector3 calculateOrbit(double a, double e, double i, double Omega, double omega, double period, double t)
{
    double M = 2 * PI * t / period;
    double E = solve_kepler(M, e);
    double x = a * (cos(E) - e);
    double y = a * sqrt(1 - e*e) * sin(E);
    
    double x_omega = x * cos(omega) - y * sin(omega);
    double y_omega = x * sin(omega) + y * cos(omega);
    double z_incl = y_omega * sin(i);
    double y_incl = y_omega * cos(i);
    double x_final = x_omega * cos(Omega) - y_incl * sin(Omega);
    double y_final = x_omega * sin(Omega) + y_incl * cos(Omega);
    
    return (Vector3){(float)x_final, (float)z_incl, (float)y_final};
}

// ================= CREATE ATMOSPHERE =================
static Model CreateAtmosphereModel(float radius, Color color, float alpha)
{
    Mesh mesh = GenMeshSphere(radius, 32, 32);
    Model model = LoadModelFromMesh(mesh);
    UnloadMesh(mesh);
    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = (Color){color.r, color.g, color.b, (unsigned char)alpha};
    return model;
}

// ================= CREATE RING MESH =================
static Mesh CreateRingMesh(float innerRadius, float outerRadius, int segments)
{
    Mesh mesh = {0};
    int vertexCount = segments * 2;
    int triangleCount = segments * 2;
    
    mesh.vertexCount = vertexCount;
    mesh.triangleCount = triangleCount;
    mesh.vertices = (float *)malloc(vertexCount * 3 * sizeof(float));
    mesh.texcoords = (float *)malloc(vertexCount * 2 * sizeof(float));
    mesh.indices = (unsigned short *)malloc(triangleCount * 3 * sizeof(unsigned short));
    
    for (int i = 0; i < segments; i++) {
        float angle = (float)i / segments * 2 * PI;
        float cosA = cosf(angle);
        float sinA = sinf(angle);
        
        mesh.vertices[i * 6 + 0] = cosA * innerRadius;
        mesh.vertices[i * 6 + 1] = 0;
        mesh.vertices[i * 6 + 2] = sinA * innerRadius;
        mesh.texcoords[i * 4 + 0] = (float)i / segments;
        mesh.texcoords[i * 4 + 1] = 0;
        
        mesh.vertices[i * 6 + 3] = cosA * outerRadius;
        mesh.vertices[i * 6 + 4] = 0;
        mesh.vertices[i * 6 + 5] = sinA * outerRadius;
        mesh.texcoords[i * 4 + 2] = (float)i / segments;
        mesh.texcoords[i * 4 + 3] = 1;
        
        int next = (i + 1) % segments;
        mesh.indices[i * 6 + 0] = i * 2;
        mesh.indices[i * 6 + 1] = next * 2;
        mesh.indices[i * 6 + 2] = i * 2 + 1;
        mesh.indices[i * 6 + 3] = next * 2;
        mesh.indices[i * 6 + 4] = next * 2 + 1;
        mesh.indices[i * 6 + 5] = i * 2 + 1;
    }
    UploadMesh(&mesh, false);
    return mesh;
}

// ================= CREATE SUN GLOW =================
static Texture2D CreateSunGlowTexture() {
    Image glowImg = GenImageColor(256, 256, BLANK);
    ImageDrawCircle(&glowImg, 128, 128, 100, (Color){255, 200, 100, 60});
    ImageDrawCircle(&glowImg, 128, 128, 70, (Color){255, 210, 120, 80});
    ImageDrawCircle(&glowImg, 128, 128, 40, (Color){255, 220, 140, 100});
    Texture2D tex = LoadTextureFromImage(glowImg);
    UnloadImage(glowImg);
    return tex;
}

// ================= DRAW INFO PANEL =================
static void DrawInfoPanel(SelectionInfo *info, Planet *planets)
{
    if (!info->active) return;
    
    info->scrollOffset += (info->targetScroll - info->scrollOffset) * 0.2f;
    
    float panelWidth = 500;
    float panelHeight = 550;
    float panelX = (GetScreenWidth() - panelWidth) / 2;
    float panelY = (GetScreenHeight() - panelHeight) / 2;
    
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0, 0, 0, 200});
    DrawRectangleRounded((Rectangle){panelX, panelY, panelWidth, panelHeight}, 0.1f, 10, (Color){20, 20, 45, 250});
    DrawRectangleRoundedLines((Rectangle){panelX, panelY, panelWidth, panelHeight}, 0.1f, 10, GOLD);
    DrawRectangleRounded((Rectangle){panelX + 5, panelY + 5, panelWidth - 10, 50}, 0.1f, 10, (Color){60, 60, 100, 255});
    
    char title[64];
    Color titleColor;
    if (info->selectedType == 0) {
        sprintf(title, "★ %s ★", planets[info->selectedIndex].name);
        titleColor = YELLOW;
    } else if (info->selectedType == 1) {
        sprintf(title, "★ SUN ★");
        titleColor = ORANGE;
    } else {
        sprintf(title, "★ MOON ★");
        titleColor = LIGHTGRAY;
    }
    int titleWidth = MeasureText(title, 26);
    DrawText(title, panelX + panelWidth/2 - titleWidth/2, panelY + 18, 26, titleColor);
    
    Rectangle closeBtn = {panelX + panelWidth - 45, panelY + 10, 35, 35};
    DrawRectangleRounded(closeBtn, 0.2f, 10, (Color){200, 60, 60, 220});
    DrawText("X", closeBtn.x + 12, closeBtn.y + 7, 22, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), closeBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        info->active = false;
        return;
    }
    
    float contentY = panelY + 65;
    float contentHeight = panelHeight - 120;
    BeginScissorMode(panelX + 15, contentY, panelWidth - 30, contentHeight);
    
    float y = contentY + 10 - info->scrollOffset;
    
    float previewX = panelX + panelWidth/2 - 60;
    if (info->selectedType == 0 && planets[info->selectedIndex].tex.id != 0) {
        DrawTexturePro(planets[info->selectedIndex].tex, 
            (Rectangle){0, 0, (float)planets[info->selectedIndex].tex.width, (float)planets[info->selectedIndex].tex.height},
            (Rectangle){previewX, y, 120, 120}, (Vector2){0, 0}, 0, WHITE);
    } else if (info->selectedType == 1) {
        DrawCircle(previewX + 60, y + 60, 45, ORANGE);
        DrawCircle(previewX + 60, y + 60, 30, YELLOW);
        DrawCircle(previewX + 60, y + 60, 15, (Color){255, 220, 100, 255});
    } else {
        DrawCircle(previewX + 60, y + 60, 40, LIGHTGRAY);
        DrawCircle(previewX + 60, y + 60, 25, GRAY);
        DrawCircle(previewX + 60, y + 60, 12, DARKGRAY);
    }
    y += 135;
    
    DrawText("DESCRIPTION", panelX + 20, y, 16, SKYBLUE);
    y += 22;
    DrawRectangle(panelX + 15, y, panelWidth - 30, 55, (Color){0, 0, 0, 120});
    
    if (info->selectedType == 0) {
        int idx = info->selectedIndex;
        const char* desc = idx == 0 ? "The smallest and fastest planet, closest to the Sun." :
                          idx == 1 ? "Earth's 'sister planet' with a thick toxic atmosphere." :
                          idx == 2 ? "Our home, the only known planet to support life." :
                          idx == 3 ? "The Red Planet, named after the Roman god of war." :
                          idx == 4 ? "The largest planet in our solar system, a gas giant." :
                          idx == 5 ? "Famous for its beautiful ring system." :
                          idx == 6 ? "Rotates on its side, giving it extreme seasons." : 
                          "The windiest planet in the solar system.";
        DrawText(desc, panelX + 20, y + 8, 12, LIGHTGRAY);
    } else if (info->selectedType == 1) {
        DrawText("The Sun is the star at the center of our Solar System.", panelX + 20, y + 5, 12, LIGHTGRAY);
        DrawText("It is a nearly perfect sphere of hot plasma, heated by", panelX + 20, y + 22, 12, LIGHTGRAY);
        DrawText("nuclear fusion reactions in its core.", panelX + 20, y + 39, 12, LIGHTGRAY);
    } else {
        DrawText("The Moon is Earth's only natural satellite.", panelX + 20, y + 5, 12, LIGHTGRAY);
        DrawText("It is the fifth-largest natural satellite in the Solar", panelX + 20, y + 22, 12, LIGHTGRAY);
        DrawText("System and is in synchronous rotation with Earth.", panelX + 20, y + 39, 12, LIGHTGRAY);
    }
    y += 70;
    
    DrawText("STATISTICS", panelX + 20, y, 16, SKYBLUE);
    y += 22;
    
    if (info->selectedType == 0) {
        Planet *p = &planets[info->selectedIndex];
        DrawText(TextFormat("Distance from Sun: %.2f AU (%.0f million km)", p->a, p->a * 149.6), panelX + 25, y, 12, WHITE); y += 18;
        DrawText(TextFormat("Orbital Period: %.2f Earth years", p->period), panelX + 25, y, 12, WHITE); y += 18;
        DrawText(TextFormat("Diameter: %.0f km", p->radius * 20000), panelX + 25, y, 12, WHITE); y += 18;
        DrawText(TextFormat("Day Length: %.1f Earth hours", 24.0f / fmax(p->rot_speed, 0.1f)), panelX + 25, y, 12, WHITE); y += 18;
        DrawText(TextFormat("Axial Tilt: %.1f°", p->tilt), panelX + 25, y, 12, WHITE); y += 25;
    } else if (info->selectedType == 1) {
        DrawText("Type: G-type main-sequence star", panelX + 25, y, 12, WHITE); y += 18;
        DrawText("Age: ~4.6 billion years", panelX + 25, y, 12, WHITE); y += 18;
        DrawText("Diameter: 1.39 million km", panelX + 25, y, 12, WHITE); y += 18;
        DrawText("Surface Temperature: ~5,500°C", panelX + 25, y, 12, WHITE); y += 18;
        DrawText("Mass: 333,000 x Earth", panelX + 25, y, 12, WHITE); y += 25;
    } else {
        DrawText("Type: Natural satellite", panelX + 25, y, 12, WHITE); y += 18;
        DrawText("Distance from Earth: 384,400 km", panelX + 25, y, 12, WHITE); y += 18;
        DrawText("Diameter: 3,474 km", panelX + 25, y, 12, WHITE); y += 18;
        DrawText("Orbital Period: 27.3 days", panelX + 25, y, 12, WHITE); y += 18;
        DrawText("Gravity: 1.62 m/s² (0.165 x Earth)", panelX + 25, y, 12, WHITE); y += 25;
    }
    
    DrawText("INTERESTING FACTS", panelX + 20, y, 16, SKYBLUE);
    y += 22;
    
    if (info->selectedType == 0) {
        int idx = info->selectedIndex;
        if (idx == 0) {
            DrawText("• Extreme temperatures: -173°C to 427°C", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• No atmosphere to retain heat", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• One day is longer than one year!", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• Most cratered planet", panelX + 25, y, 11, LIME); y += 20;
        } else if (idx == 1) {
            DrawText("• Hottest planet (462°C average)", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• Rotates backwards (retrograde)", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• Atmosphere is 96% CO2", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• Pressure 92x Earth's", panelX + 25, y, 11, LIME); y += 20;
        } else if (idx == 2) {
            DrawText("• Only known planet with life", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• 71% covered in water", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• Protective magnetic field", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• One moon (Luna)", panelX + 25, y, 11, LIME); y += 20;
        } else if (idx == 3) {
            DrawText("• Home to Olympus Mons (largest volcano)", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• Called the Red Planet", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• Two small moons: Phobos & Deimos", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• Thin CO2 atmosphere", panelX + 25, y, 11, LIME); y += 20;
        } else if (idx == 4) {
            DrawText("• Largest planet in solar system", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• Great Red Spot (storm bigger than Earth)", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• 79 known moons", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• Gas giant - no solid surface", panelX + 25, y, 11, LIME); y += 20;
        } else if (idx == 5) {
            DrawText("• Famous for beautiful ring system", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• Least dense - would float in water", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• 82 confirmed moons", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• Rings of ice and rock", panelX + 25, y, 11, LIME); y += 20;
        } else if (idx == 6) {
            DrawText("• Rotates on its side (98° tilt)", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• First telescope discovery", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• 27 known moons", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• Methane gives blue-green color", panelX + 25, y, 11, LIME); y += 20;
        } else {
            DrawText("• Strongest winds (2,100 km/h)", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• 14 known moons", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• Predicted mathematically", panelX + 25, y, 11, LIME); y += 16;
            DrawText("• Triton orbits backwards", panelX + 25, y, 11, LIME); y += 20;
        }
    } else if (info->selectedType == 1) {
        DrawText("• Contains 99.86% of Solar System's mass", panelX + 25, y, 11, LIME); y += 16;
        DrawText("• Light takes 8 minutes to reach Earth", panelX + 25, y, 11, LIME); y += 16;
        DrawText("• Will become a red giant in ~5 billion years", panelX + 25, y, 11, LIME); y += 16;
        DrawText("• Solar flares can disrupt communications", panelX + 25, y, 11, LIME); y += 20;
    } else {
        DrawText("• Moving away from Earth at 3.8 cm/year", panelX + 25, y, 11, LIME); y += 16;
        DrawText("• Only 12 people have walked on it", panelX + 25, y, 11, LIME); y += 16;
        DrawText("• No atmosphere - footprints last millions of years", panelX + 25, y, 11, LIME); y += 16;
        DrawText("• Causes Earth's tides", panelX + 25, y, 11, LIME); y += 20;
    }
    
    EndScissorMode();
    
    float totalContent = y - (contentY + 10 - info->scrollOffset);
    float scrollRatio = contentHeight / totalContent;
    if (scrollRatio < 1.0f && totalContent > 0) {
        float barHeight = fmax(contentHeight * scrollRatio, 30);
        float barY = contentY + (info->scrollOffset / (totalContent - contentHeight)) * (contentHeight - barHeight);
        DrawRectangleRounded((Rectangle){panelX + panelWidth - 20, barY, 8, barHeight}, 0.5f, 10, (Color){150, 150, 200, 180});
    }
    
    Rectangle scrollArea = {panelX + 15, contentY, panelWidth - 30, contentHeight};
    if (CheckCollisionPointRec(GetMousePosition(), scrollArea)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            info->targetScroll -= wheel * 30;
            if (info->targetScroll < 0) info->targetScroll = 0;
            float maxScroll = fmax(totalContent - contentHeight, 0);
            if (info->targetScroll > maxScroll) info->targetScroll = maxScroll;
        }
    }
    
    float btnY = panelY + panelHeight - 55;
    Rectangle closeBtn2 = {panelX + panelWidth/2 - 60, btnY, 120, 40};
    DrawRectangleRounded(closeBtn2, 0.2f, 10, (Color){80, 80, 120, 230});
    DrawRectangleRoundedLines(closeBtn2, 0.2f, 10, (Color){200, 200, 255, 150});
    DrawText("CLOSE", closeBtn2.x + 38, closeBtn2.y + 10, 18, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), closeBtn2) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        info->active = false;
    }
}

int main(void)
{
    InitWindow(1920, 1080, "SOLAR SYSTEM SIMULATION - BETA 1.0");
    SetTargetFPS(MAX_FPS);
    srand(time(NULL));
    
    Camera3D cam = {0};
    cam.position = (Vector3){0, 50, 85};
    cam.target = (Vector3){0, 0, 0};
    cam.up = (Vector3){0, 1, 0};
    cam.fovy = 65;
    
    Mesh sphere = GenMeshSphere(1, 48, 48);
    Mesh lowPoly = GenMeshSphere(1, 24, 24);
    
    printf("Loading textures...\n");
    Texture2D sunTex = LoadTexture("sun.png");
    Texture2D mercTex = LoadTexture("mercury.png");
    Texture2D venTex = LoadTexture("venus.png");
    Texture2D earthTex = LoadTexture("earth.png");
    Texture2D cloudTex = LoadTexture("earth_clouds.png");
    Texture2D moonTex = LoadTexture("moon.png");
    Texture2D marsTex = LoadTexture("mars.png");
    Texture2D jupTex = LoadTexture("jupiter.png");
    Texture2D satTex = LoadTexture("saturn.png");
    Texture2D ringTex = LoadTexture("saturn_ring.png");
    Texture2D uraTex = LoadTexture("uranus.png");
    Texture2D nepTex = LoadTexture("neptune.png");
    Texture2D starsTex = LoadTexture("stars.png");
    
    Texture2D sunGlow = CreateSunGlowTexture();
   
    Texture2D asteroidTex = LoadTexture("asteroid.png");
    Model asteroidModel = {0};
    if (asteroidTex.id != 0) {
        Mesh sphereMesh = GenMeshSphere(1.0f, 16, 16);
        asteroidModel = LoadModelFromMesh(sphereMesh);
        SetMaterialTexture(&asteroidModel.materials[0], MATERIAL_MAP_DIFFUSE, asteroidTex);
        UnloadMesh(sphereMesh);
        printf("✓ Asteroid model loaded with texture\n");
    } else {
        printf("Warning: asteroid.png not found\n");
    }

    Planet planets[MAX_PLANETS];
    memset(planets, 0, sizeof(planets));
    
    for (int i = 0; i < MAX_PLANETS; i++) {
        strcpy(planets[i].name, orbitalData[i].name);
        planets[i].a = orbitalData[i].a;
        planets[i].e = orbitalData[i].e;
        planets[i].i = orbitalData[i].i;
        planets[i].Omega = orbitalData[i].Omega;
        planets[i].omega = orbitalData[i].omega;
        planets[i].period = orbitalData[i].period;
        planets[i].radius = orbitalData[i].radius;
        planets[i].tilt = orbitalData[i].tilt;
        planets[i].rot_speed = orbitalData[i].rot_speed;
        planets[i].atmosphereHeight = orbitalData[i].atmosphereHeight;
        planets[i].atmosphereColor = orbitalData[i].atmosphereColor;
        planets[i].atmosphereAlpha = orbitalData[i].atmosphereAlpha;
        
        switch(i) {
            case 0: planets[i].tex = mercTex; break;
            case 1: planets[i].tex = venTex; break;
            case 2: planets[i].tex = earthTex; planets[i].cloudTex = cloudTex; break;
            case 3: planets[i].tex = marsTex; break;
            case 4: planets[i].tex = jupTex; break;
            case 5: planets[i].tex = satTex; break;
            case 6: planets[i].tex = uraTex; break;
            case 7: planets[i].tex = nepTex; break;
        }
        
        planets[i].model = LoadModelFromMesh(sphere);
        if (planets[i].tex.id != 0) {
            SetMaterialTexture(&planets[i].model.materials[0], MATERIAL_MAP_DIFFUSE, planets[i].tex);
        }
        
        if (planets[i].atmosphereHeight > 0) {
            planets[i].atmosphereModel = CreateAtmosphereModel(1 + planets[i].atmosphereHeight, 
                                                               planets[i].atmosphereColor, 
                                                               planets[i].atmosphereAlpha);
        }
        
        if (i == 2 && planets[i].cloudTex.id != 0) {
            planets[i].cloudModel = LoadModelFromMesh(lowPoly);
            SetMaterialTexture(&planets[i].cloudModel.materials[0], MATERIAL_MAP_DIFFUSE, planets[i].cloudTex);
        }
    }
    
    Model sunModel = LoadModelFromMesh(sphere);
    SetMaterialTexture(&sunModel.materials[0], MATERIAL_MAP_DIFFUSE, sunTex);
    
    Model moonModel = LoadModelFromMesh(lowPoly);
    SetMaterialTexture(&moonModel.materials[0], MATERIAL_MAP_DIFFUSE, moonTex);
    
    Mesh ringMesh = CreateRingMesh(1.2f, 2.0f, 64);
    Model ringModel = LoadModelFromMesh(ringMesh);
    SetMaterialTexture(&ringModel.materials[0], MATERIAL_MAP_DIFFUSE, ringTex);
    ringModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = (Color){255, 245, 200, 200};
    
    Mesh outerRing = CreateRingMesh(2.1f, 2.6f, 64);
    Model outerRingModel = LoadModelFromMesh(outerRing);
    SetMaterialTexture(&outerRingModel.materials[0], MATERIAL_MAP_DIFFUSE, ringTex);
    outerRingModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = (Color){200, 190, 160, 160};
    
    Asteroid asteroids[ASTEROID_COUNT];
    
    int largeCount = 0, mediumCount = 0, smallCount = 0;
    for (int i = 0; i < ASTEROID_COUNT; i++) {
        asteroids[i].radius = 6.5f + ((float)rand() / RAND_MAX) * 3.0f;
        asteroids[i].angle = ((float)rand() / RAND_MAX) * 2 * PI;
        asteroids[i].yOffset = ((float)rand() / RAND_MAX - 0.5f) * 0.6f;
        asteroids[i].speed = 0.06f + ((float)rand() / RAND_MAX) * 0.1f;
        asteroids[i].rotSpeed = 0.5f + ((float)rand() / RAND_MAX) * 2.0f;
        asteroids[i].rotation = ((float)rand() / RAND_MAX) * 2 * PI;
        
        float sizeRand = (float)rand() / RAND_MAX;
        if (sizeRand < 0.1f) {
            asteroids[i].size = 0.12f + ((float)rand() / RAND_MAX) * 0.08f;
            largeCount++;
        } else if (sizeRand < 0.4f) {
            asteroids[i].size = 0.07f + ((float)rand() / RAND_MAX) * 0.05f;
            mediumCount++;
        } else {
            asteroids[i].size = 0.04f + ((float)rand() / RAND_MAX) * 0.03f;
            smallCount++;
        }
        
        int colorType = rand() % 6;
        switch(colorType) {
            case 0: asteroids[i].color = (Color){160, 140, 110, 220}; break; // Light brown
            case 1: asteroids[i].color = (Color){140, 120, 90, 220}; break;  // Medium brown
            case 2: asteroids[i].color = (Color){180, 150, 110, 220}; break; // Warm brown
            case 3: asteroids[i].color = (Color){120, 100, 75, 220}; break;   // Dark brown
            case 4: asteroids[i].color = (Color){100, 90, 80, 220}; break;    // Gray-brown
            default: asteroids[i].color = (Color){150, 130, 100, 220}; break; // Tan
        }
        
        if (rand() % 100 < 15) {
            asteroids[i].color = (Color){180, 100, 70, 220};
        }
        
        if (rand() % 100 < 10) {
            asteroids[i].color = (Color){170, 160, 150, 220};
        }
    }

    float zoom = 85.0f;
    float minZoom = 5.0f;
    float maxZoom = 200.0f;
    float camAngleX = 0.3f, camAngleY = 0.4f;
    bool rotating = false;
    Vector2 lastMouse = {0, 0};
    
    Vector3 moonPos = {0, 0, 0};
    float moonRadius = 0.18f;
    
    SelectionInfo selection = {0};
    selection.active = false;
    
    float timeScale = 0.2f;
    float starRot = 0;
    bool showOrbits = true;
    bool showTrails = true;
    bool showAtmospheres = true;
    
    #define STAR_COUNT 300
    Vector2 stars[STAR_COUNT];
    for (int i = 0; i < STAR_COUNT; i++) {
        stars[i] = (Vector2){ (float)(rand() % GetScreenWidth()), (float)(rand() % GetScreenHeight()) };
    }
    
    float fps = 60;
    int frameCnt = 0;
    float frameTime = 0;
    
    printf("\n========================================\n");
    printf("SOLAR SYSTEM SIMULATION - BETA 1.0\n");
    printf("Click on any planet, Sun, or Moon for info!\n");
    printf("========================================\n\n");
    
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.033f) dt = 0.033f;
        
        frameTime += dt;
        frameCnt++;
        if (frameTime >= 1.0f) {
            fps = frameCnt;
            frameCnt = 0;
            frameTime = 0;
        }
        
        float t = GetTime() * timeScale;
        
        for (int i = 0; i < MAX_PLANETS; i++) {
            planets[i].position = calculateOrbit(planets[i].a, planets[i].e, planets[i].i,
                                                planets[i].Omega, planets[i].omega,
                                                planets[i].period, t);
            if (showTrails) {
                planets[i].trail[planets[i].trailIndex] = planets[i].position;
                planets[i].trailIndex = (planets[i].trailIndex + 1) % TRAIL_LENGTH;
            }
        }
        
        float moonAngle = t * 13.0f;
        moonPos = Vector3Add(planets[2].position, 
                            (Vector3){cosf(moonAngle) * 0.75f, sinf(moonAngle) * 0.1f, sinf(moonAngle) * 0.55f});
        
        for (int i = 0; i < ASTEROID_COUNT; i++) {
            asteroids[i].angle += asteroids[i].speed * dt;
            if (asteroids[i].angle > 2 * PI) asteroids[i].angle -= 2 * PI;
            
            asteroids[i].rotation += asteroids[i].rotSpeed * dt;
            if (asteroids[i].rotation > 2 * PI) asteroids[i].rotation -= 2 * PI;
            
            float verticalWobble = sinf(asteroids[i].angle * 2.5f) * 0.08f;
            float horizontalWobble = cosf(asteroids[i].angle * 1.8f) * 0.03f;
            
            asteroids[i].position = (Vector3){
                cosf(asteroids[i].angle + horizontalWobble) * asteroids[i].radius,
                asteroids[i].yOffset + verticalWobble,
                sinf(asteroids[i].angle + horizontalWobble) * asteroids[i].radius
            };
        }

        float wheel = GetMouseWheelMove();
        if (wheel != 0 && !selection.active) {
            zoom -= wheel * 3.0f;
            if (zoom < minZoom) zoom = minZoom;
            if (zoom > maxZoom) zoom = maxZoom;
        }
        
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !selection.active) {
            if (!rotating) {
                rotating = true;
                lastMouse = GetMousePosition();
            }
            Vector2 cur = GetMousePosition();
            Vector2 delta = {cur.x - lastMouse.x, cur.y - lastMouse.y};
            camAngleX += delta.x * 0.006f;
            camAngleY += delta.y * 0.006f;
            if (camAngleY > 1.2f) camAngleY = 1.2f;
            if (camAngleY < 0.15f) camAngleY = 0.15f;
            lastMouse = cur;
        } else {
            rotating = false;
        }
        
        cam.position = (Vector3){
            cosf(camAngleX) * cosf(camAngleY) * zoom,
            sinf(camAngleY) * zoom + 10,
            sinf(camAngleX) * cosf(camAngleY) * zoom
        };
        cam.target = (Vector3){0, 0, 0};
        
        // ================= FIXED PLANET SELECTION USING SCREEN SPACE =================
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (selection.active) {
                selection.active = false;
            } else {
                Vector2 mousePos = GetMousePosition();
                int bestMatch = -1;
                int bestType = -1;
                float bestDist = 50.0f; // Maximum pixel distance for selection
                
                // Check Sun (screen position)
                Vector3 sunPos = {0, 0, 0};
                Vector2 sunScreen = GetWorldToScreen(sunPos, cam);
                float sunDist = Vector2Distance(mousePos, sunScreen);
                if (sunDist < bestDist) {
                    bestDist = sunDist;
                    bestType = 1;
                    bestMatch = -1;
                }
                
                // Check Moon
                Vector2 moonScreen = GetWorldToScreen(moonPos, cam);
                float moonDist = Vector2Distance(mousePos, moonScreen);
                if (moonDist < bestDist) {
                    bestDist = moonDist;
                    bestType = 2;
                    bestMatch = -1;
                }
                
                // Check all planets
                for (int i = 0; i < MAX_PLANETS; i++) {
                    Vector2 planetScreen = GetWorldToScreen(planets[i].position, cam);
                    float dist = Vector2Distance(mousePos, planetScreen);
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestType = 0;
                        bestMatch = i;
                    }
                }
                
                if (bestType >= 0 && bestDist < 50.0f) {
                    selection.active = true;
                    selection.selectedType = bestType;
                    selection.selectedIndex = bestMatch;
                    selection.scrollOffset = 0;
                    selection.targetScroll = 0;
                    if (bestType == 0) printf("✓ SELECTED: %s (distance: %.1f pixels)\n", planets[bestMatch].name, bestDist);
                    else if (bestType == 1) printf("✓ SELECTED: SUN (distance: %.1f pixels)\n", bestDist);
                    else printf("✓ SELECTED: MOON (distance: %.1f pixels)\n", bestDist);
                }
            }
        }
        
        BeginDrawing();
        ClearBackground(BLACK);
        
        for (int i = 0; i < STAR_COUNT; i++) {
            DrawPixelV(stars[i], (Color){120, 120, 180, 150});
        }
        
        if (starsTex.id != 0) {
            starRot += 0.0003f * dt;
            DrawTexturePro(starsTex, (Rectangle){0, 0, (float)starsTex.width, (float)starsTex.height},
                          (Rectangle){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()},
                          (Vector2){0, 0}, starRot, (Color){255, 255, 255, 20});
        }
        
        BeginMode3D(cam);
        
        if (showOrbits) {
            for (int i = 0; i < MAX_PLANETS; i++) {
                DrawCircle3D((Vector3){0, 0, 0}, planets[i].a, (Vector3){0, 1, 0}, 90, (Color){80, 80, 150, 40});
            }
        }
        
        float sunPulse = 1.0f + 0.02f * sinf(GetTime() * 2.0f);
        DrawBillboard(cam, sunGlow, (Vector3){0,0,0}, 2.8f * sunPulse, (Color){255, 200, 100, 80});
        DrawModelEx(sunModel, (Vector3){0,0,0}, (Vector3){0,1,0}, GetTime() * 0.15f,
                   (Vector3){1.4f * sunPulse, 1.4f * sunPulse, 1.4f * sunPulse}, WHITE);
        
        if (selection.active && selection.selectedType == 1) {
            DrawSphere((Vector3){0,0,0}, 1.6f, (Color){255, 255, 100, 120});
        }
        
        if (asteroidTex.id != 0) {
            for (int i = 0; i < ASTEROID_COUNT; i += 2) {
                float distanceToSun = sqrtf(asteroids[i].position.x * asteroids[i].position.x +
                                            asteroids[i].position.z * asteroids[i].position.z);
                float brightness = 0.6f + (1.0f / (1.0f + distanceToSun * 0.08f));
                if (brightness > 1.0f) brightness = 1.0f;
                if (brightness < 0.4f) brightness = 0.4f;
                
                Vector3 toCamera = Vector3Subtract(cam.position, asteroids[i].position);
                toCamera = Vector3Normalize(toCamera);
                
                DrawBillboard(cam, asteroidTex, asteroids[i].position,
                             asteroids[i].size * 1.2f, 
                             (Color){220, 190, 140, (unsigned char)(200 * brightness)});
            }
        }

        for (int i = 0; i < MAX_PLANETS; i++) {
            Planet *p = &planets[i];
            float rotAngle = GetTime() * p->rot_speed;
            
            if (selection.active && selection.selectedType == 0 && selection.selectedIndex == i) {
                DrawSphere(p->position, p->radius + 0.2f, (Color){255, 255, 100, 120});
                DrawCircle3D(p->position, p->radius + 0.4f, (Vector3){0, 1, 0}, 30, (Color){255, 255, 100, 200});
            }
            
            if (showTrails) {
                for (int j = 0; j < TRAIL_LENGTH - 1; j++) {
                    int idx = (p->trailIndex - j - 1 + TRAIL_LENGTH) % TRAIL_LENGTH;
                    int nxt = (p->trailIndex - j + TRAIL_LENGTH) % TRAIL_LENGTH;
                    float alpha = 1.0f - (float)j / TRAIL_LENGTH;
                    DrawLine3D(p->trail[idx], p->trail[nxt], (Color){100, 150, 255, (unsigned char)(40 * alpha)});
                }
            }
            
            if (showAtmospheres && p->atmosphereHeight > 0 && p->atmosphereModel.materialCount > 0) {
                DrawModelEx(p->atmosphereModel, p->position, (Vector3){1, 0, 0}, 0,
                           (Vector3){1 + p->atmosphereHeight * 0.3f, 
                                    1 + p->atmosphereHeight * 0.3f,
                                    1 + p->atmosphereHeight * 0.3f}, WHITE);
            }
            
            DrawModelEx(p->model, p->position, (Vector3){0,1,0}, rotAngle,
                       (Vector3){p->radius, p->radius, p->radius}, WHITE);
            
            if (i == 2 && p->cloudModel.materialCount > 0) {
                float cloudRot = rotAngle * 1.2f;
                DrawModelEx(p->cloudModel, p->position, (Vector3){0,1,0}, cloudRot,
                           (Vector3){p->radius * 1.03f, p->radius * 1.03f, p->radius * 1.03f},
                           (Color){255, 255, 255, 140});
                
                DrawModelEx(moonModel, moonPos, (Vector3){0,1,0}, GetTime() * 2.0f,
                           (Vector3){moonRadius, moonRadius, moonRadius}, WHITE);
                
                if (selection.active && selection.selectedType == 2) {
                    DrawSphere(moonPos, moonRadius + 0.15f, (Color){255, 255, 100, 120});
                }
            }
            
            if (i == 5) {
                DrawModelEx(ringModel, p->position, (Vector3){1,0,0}, 28.0f,
                           (Vector3){1.0f, 1.0f, 0.25f}, (Color){255, 245, 200, 200});
                DrawModelEx(outerRingModel, p->position, (Vector3){1,0,0}, 25.0f,
                           (Vector3){1.0f, 1.0f, 0.15f}, (Color){200, 190, 160, 160});
            }
            
            Vector2 sp = GetWorldToScreen(p->position, cam);
            float dist = Vector3Distance(p->position, cam.position);
            if (dist < 400) {
                int fs = (dist < 150) ? 12 : 10;
                DrawRectangle(sp.x - 35, sp.y - 18, 70, 16, (Color){0, 0, 0, 180});
                DrawText(p->name, sp.x - MeasureText(p->name, fs)/2, sp.y - 16, fs, YELLOW);
            }
        }
        
        Vector2 moonSp = GetWorldToScreen(moonPos, cam);
        float moonDistCam = Vector3Distance(moonPos, cam.position);
        if (moonDistCam < 300) {
            DrawRectangle(moonSp.x - 30, moonSp.y - 18, 60, 16, (Color){0, 0, 0, 180});
            DrawText("Moon", moonSp.x - 18, moonSp.y - 16, 11, LIGHTGRAY);
        }
        
        EndMode3D();
        
        if (!selection.active) {
            DrawText("SOLAR SYSTEM SIMULATION - BETA 1.0", 10, 10, 20, YELLOW);
            DrawText(TextFormat("FPS: %.0f", fps), 10, 40, 15, fps > 55 ? GREEN : RED);
            DrawText("★ LEFT CLICK on any planet, Sun, or Moon for DETAILED INFORMATION ★", 10, 65, 14, LIME);
            DrawText("Right-click+Drag: Rotate Camera | Mouse Wheel: Zoom In/Out", 10, 90, 12, LIGHTGRAY);
            DrawText(TextFormat("Time Scale: %.2fx | Zoom: %.1f", timeScale, zoom), 10, 110, 12, SKYBLUE);
            
            if (showAtmospheres) DrawText("Atmospheres: ON (Press A to toggle)", 10, 130, 12, GREEN);
            else DrawText("Atmospheres: OFF (Press A to toggle)", 10, 130, 12, RED);
            
            DrawText("TOGGLES:", 10, GetScreenHeight() - 90, 14, WHITE);
            DrawText("O: Orbits", 10, GetScreenHeight() - 70, 11, showOrbits ? GREEN : RED);
            DrawText("T: Trails", 80, GetScreenHeight() - 70, 11, showTrails ? GREEN : RED);
            DrawText("A: Atmospheres", 150, GetScreenHeight() - 70, 11, showAtmospheres ? GREEN : RED);
            DrawText("R: Reset Speed", 260, GetScreenHeight() - 70, 11, LIGHTGRAY);
            DrawText("UP/DOWN: Change Speed", 370, GetScreenHeight() - 70, 11, LIGHTGRAY);
            
            DrawText("PLANET DISTANCES (AU):", 10, GetScreenHeight() - 45, 12, SKYBLUE);
            DrawText(TextFormat("Mercury:%.1f Venus:%.1f Earth:%.1f Mars:%.1f Jupiter:%.1f Saturn:%.1f", 
                    planets[0].a, planets[1].a, planets[2].a, planets[3].a, planets[4].a, planets[5].a), 
                    10, GetScreenHeight() - 28, 11, LIGHTGRAY);
        }
        
        DrawInfoPanel(&selection, planets);
        
        EndDrawing();
        
        if (!selection.active) {
            if (IsKeyPressed(KEY_UP)) timeScale = fmin(timeScale * 1.5f, 30.0f);
            if (IsKeyPressed(KEY_DOWN)) timeScale = fmax(timeScale / 1.5f, 0.05f);
            if (IsKeyPressed(KEY_R)) timeScale = 0.2f;
            if (IsKeyPressed(KEY_O)) showOrbits = !showOrbits;
            if (IsKeyPressed(KEY_T)) showTrails = !showTrails;
            if (IsKeyPressed(KEY_A)) showAtmospheres = !showAtmospheres;
        }
    }
    
    UnloadTexture(sunTex);
    UnloadTexture(sunGlow);
    UnloadTexture(mercTex);
    UnloadTexture(venTex);
    UnloadTexture(earthTex);
    UnloadTexture(cloudTex);
    UnloadTexture(moonTex);
    UnloadTexture(marsTex);
    UnloadTexture(jupTex);
    UnloadTexture(satTex);
    UnloadTexture(ringTex);
    UnloadTexture(uraTex);
    UnloadTexture(nepTex);
    UnloadTexture(starsTex);
    UnloadTexture(asteroidTex);    
    if (asteroidModel.materialCount > 0) UnloadModel(asteroidModel);

    UnloadMesh(sphere);
    UnloadMesh(lowPoly);
    UnloadMesh(ringMesh);
    UnloadMesh(outerRing);
    
    UnloadModel(sunModel);
    for (int i = 0; i < MAX_PLANETS; i++) {
        UnloadModel(planets[i].model);
        if (planets[i].atmosphereModel.materialCount > 0) UnloadModel(planets[i].atmosphereModel);
        if (planets[i].cloudModel.materialCount > 0) UnloadModel(planets[i].cloudModel);
    }
    UnloadModel(moonModel);
    UnloadModel(ringModel);
    UnloadModel(outerRingModel);
    
    CloseWindow();
    printf("Simulation ended.\n");
    return 0;
}
