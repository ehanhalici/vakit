#include <cstdio>
#include <stdio.h>
#include <time.h>
#include <cmath>
#include <algorithm>
#include "imgui/misc/single_file/imgui_single_file.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "ImGuiDatePicker/ImGuiDatePicker.hpp"
#include <GLFW/glfw3.h>
#include "sofa/20231011/c/src/sofa.h"



#define DEG_TO_RAD (M_PI / 180.0)
#define RAD_TO_DEG (180.0 / M_PI)

// Gözlemci Konumu (Örnek: İstanbul)
#define OBSERVER_LAT  (38.72868 * DEG_TO_RAD) // Enlem
#define OBSERVER_LON  (34.38236 * DEG_TO_RAD) // Boylam
#define OBSERVER_HEIGHT 1054

char *time_zone;
double altitude, azimuth, sun_angle, sun_x, sun_y;
tm selected_tm;
int hour, minute;


#define GET_YEAR(timePoint) int(timePoint.tm_year + 1900)
#define GET_MON(timePoint) int(timePoint.tm_mon + 1)


#define GET_YEAR_PTR(timePoint) int(timePoint->tm_year + 1900)
#define GET_MON_PTR(timePoint) int(timePoint->tm_mon + 1)

void set_time() {
    time_t now = time(0);
    tm *ltm = localtime(&now);
    selected_tm = *localtime(&now);  // pointer'ı struct'a kopyala
    time_zone = (char *)ltm->tm_zone;
    //selected_tm.tm_mon += 1; 
    hour = selected_tm.tm_hour;
    minute =selected_tm.tm_min;
}


double calculate_refraction(double geometric_alt_deg, double pressure_hPa, double temperature_C) {
    // Atmosferik kırılma Güneş ışığını bükerek daha yüksek görünmesini sağlar.
    // Bu etki, düşük atmosferik basınç nedeniyle gözlemci yüksekliği ile azalır.
    // Kırılmayı yaklaşık olarak hesaplamak için Saemundsson formülünü kullanın:
    // Saemundsson formula (valid for apparent altitude > -1 degree)
    double R_arcmin = (1.02 / tan((geometric_alt_deg + 10.3 / (geometric_alt_deg + 5.11)) * DEG_TO_RAD)) 
                      * (pressure_hPa / 1010.0)  // Adjust for pressure
                      * (283.0 / (temperature_C + 273.15));  // Adjust for temperature
    return R_arcmin / 60.0;  // Convert arcminutes to degrees
}

double estimate_pressure(double height_m) {
    // Basıncı yaklaşık olarak hesaplamak için barometrik formülü kullanın
    // Barometric formula (simplified)
    return 1013.25 * exp(-height_m / 8435.2);  // Returns pressure in hPa
}

// Güneş açısını hesaplayan fonksiyon
void calculate_sun_angle() {
    double jd_utc, fjd;
    double sun_ra, sun_dec;
    double gast, last;


    // Copy local time into a tm struct
    struct tm local_tm = selected_tm;  // Assume selected_tm is your local time
    local_tm.tm_hour = hour;           // Local hour
    local_tm.tm_min = minute;          // Local minute
    local_tm.tm_sec = 0;
    local_tm.tm_isdst = -1;  // Let mktime determine daylight saving time

    // Convert local tm to time_t (UTC epoch time)
    time_t utc_time = mktime(&local_tm);
    if (utc_time == -1) {
        // Handle error: invalid time
        return;
    }

    // Convert UTC epoch time to UTC tm struct
    struct tm* utc_tm = gmtime(&utc_time);
    if (!utc_tm) {
        // Handle error
        return;
    }

    // Extract UTC components for SOFA
    int utc_year = GET_YEAR_PTR(utc_tm);  // tm_year is years since 1900
    int utc_month = GET_MON_PTR(utc_tm) ;      // tm_mon is 0-based (0=Jan)
    int utc_day = utc_tm->tm_mday;
    int utc_hour = utc_tm->tm_hour;
    int utc_min = utc_tm->tm_min;

    iauDtf2d("UTC", utc_year, utc_month, utc_day, 
             utc_hour, utc_min, 0.0, &jd_utc, &fjd);
    //iauDtf2d("UTC", GET_YEAR(utc_tm), selected_tm.tm_mon, 
    //          selected_tm.tm_mday, hour, minute, 0.0, &jd_utc, &fjd);

    // Calculate Sun's RA and Dec (invert Earth's position from Epv00)
    double pvh[2][3], pvb[2][3];
    iauEpv00(jd_utc, fjd, pvh, pvb);
    sun_ra = atan2(-pvh[0][1], -pvh[0][0]); // Invert coordinates
    double xy_sun = sqrt(pvh[0][0]*pvh[0][0] + pvh[0][1]*pvh[0][1]);
    sun_dec = atan2(-pvh[0][2], xy_sun);

    // Compute LAST
    gast = iauGmst06(jd_utc, fjd, jd_utc, fjd);
    last = gast + OBSERVER_LON; // Ensure OBSERVER_LON is in radians
    last = fmod(last, 2 * M_PI);
    if (last < 0) last += 2 * M_PI;

    // Hour angle and altitude
    double ha = last - sun_ra;
    double sin_alt = sin(OBSERVER_LAT) * sin(sun_dec) + 
                     cos(OBSERVER_LAT) * cos(sun_dec) * cos(ha);
    altitude = asin(sin_alt) * RAD_TO_DEG;

    double pressure_hPa = estimate_pressure(OBSERVER_HEIGHT);

    // (b) Assume temperature (e.g., 10°C) or pass as parameter
    double temperature_C = 10.0;

    // (c) Calculate refraction and adjust altitude
    double geometric_alt_deg = altitude;  // Your existing geometric altitude
    double refraction_deg = calculate_refraction(geometric_alt_deg, pressure_hPa, temperature_C);
    altitude = geometric_alt_deg + refraction_deg;  // Apparent altitude

    // Ensure altitude stays within [-90, 90]
    if (altitude > 90.0) altitude = 90.0;
    if (altitude < -90.0) altitude = -90.0;

    // Azimuth using atan2 for correct quadrant
    double cos_alt = cos(asin(sin_alt));
    double sin_az = -sin(ha) * cos(sun_dec);
    double cos_az = (sin(sun_dec) - sin_alt * sin(OBSERVER_LAT)) / 
                    (cos_alt * cos(OBSERVER_LAT));
    azimuth = atan2(sin_az, cos_az) * RAD_TO_DEG;
    if (azimuth < 0) azimuth += 360.0;
    
    // Altitude (Yükseklik Açısı, alt):

    // Ufuk düzlemine (yere) göre yüksekliği gösterir.
    // 0° → Tam ufukta (doğuş/batış zamanı).
    // +90° → Tam tepede (öğle vakti, Güneş tam tepe noktada).
    // -10° → Ufkun altında (Güneş batmış, gece olmuş).
    // Özet: Güneşin ne kadar yukarıda olduğunu gösterir.

    //Azimuth (Yön Açısı, az):

    // Kuzey (0°) baz alınarak saat yönünde ölçülen açı.
    // 0° → Tam kuzey.
    // 90° → Doğu.
    // 180° → Güney (Güneş öğle vakti burada olur).
    // 270° → Batı.
    // Özet: Güneşin hangi yönde olduğunu gösterir.

    // [-90, 90] -> [-1, 1]
    sun_y = altitude / 90;
    sun_x = fmod((azimuth + 90), 360);

}

void DrawSun(ImDrawList* draw_list, ImVec2 center, float radius) {
    // Açılardan x, y pozisyonu hesapla (basit kutupsal dönüşüm)
    // float angle = azm * (M_PI / 180.0f);  // Dereceyi radyana çevir
    // float height_factor = sin(alt * (M_PI / 180.0f)); // Yükseklik faktörü

    float x = center.x + radius * cos(sun_x * DEG_TO_RAD)  ;
    float y = center.y + (-1 * sun_y * radius) ; // Ekranda yukarı negatif y eksenidir
    ImGui::Text("sun X: %.2f°", sun_x);
    ImGui::Text("x: %.2f°", cos(sun_x));
    // Güneş'i çember olarak çiz
    int sun_size = 10;
    draw_list->AddCircleFilled(ImVec2(x, y), sun_size, IM_COL32(255, 255, 0, 255));
    int line_long = 250;
    draw_list->AddLine(ImVec2(center.x-line_long, center.y), ImVec2(center.x+line_long, center.y), IM_COL32(0, 255, 0, 255), sun_size*2);
}

void render_ui() {
    ImGui::Begin("Günes Acisi Hesaplayici");

    if (ImGui::DatePicker("Date", selected_tm))
    {
        //selected_tm.tm_mon += 1;
        // Perform some event whenever the date 't' is changed
    }
    //ImGui::SliderInt("Saat", &hour, 0, 23);
    //ImGui::SliderInt("Dakika", &minute, 0, 59);

// Saat ayarı
ImGui::InputInt("Saat", &hour, 1, 1, ImGuiInputTextFlags_CharsDecimal);
hour = (hour + 24) % 24;  // 0-23 sınırları içinde tut

// Dakika ayarı (otomatik saat güncelleme)
int prev_minute = minute;
ImGui::InputInt("Dakika", &minute, 1, 5, ImGuiInputTextFlags_CharsDecimal);
if (minute >= 60) {
    minute = 0;
    hour = (hour + 1) % 24;  // Dakika 60 olunca saat artar
} else if (minute < 0) {
    minute = 59;
    hour = (hour - 1 + 24) % 24;  // Dakika -1 olunca saat azalır
}
    
    // ImGui::InputInt("Saat", &hour, 1, 5, ImGuiInputTextFlags_CharsDecimal);
    // hour = (hour + 24) % 24;  // 0-23 arasında sınırla

    // ImGui::InputInt("Dakika", &minute, 1, 10, ImGuiInputTextFlags_CharsDecimal);
    // minute = (minute + 60) % 60;  // 0-59 arasında sınırla


    calculate_sun_angle();
    ImGui::Text("Tarih: %04d-%02d-%02d", GET_YEAR(selected_tm), GET_MON(selected_tm), selected_tm.tm_mday);
    ImGui::Text("Saat: %02d:%02d", selected_tm.tm_hour, selected_tm.tm_min);
    ImGui::Text("Zaman Dilimi: %s", time_zone);
    ImGui::Text("Günes Acisi: %.2f°", sun_angle);
    ImGui::Text("Altitude: %.2f°", altitude);
    ImGui::Text("Azimuth: %.2f°", azimuth);

    ImGui::End();
}

int main() {
     if (!glfwInit()) return -1;
    set_time();
    
    GLFWwindow* window = glfwCreateWindow(800, 600, "Günes Acisi", NULL, NULL);
    glfwMakeContextCurrent(window);
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
  
    float scale_factor = 1.0f;  // 4K ekran için ölçek faktörü
    ImGui::GetStyle().ScaleAllSizes(scale_factor);

    ImGuiIO& io = ImGui::GetIO();
    float font_size = 28.0f;  // Font büyüklüğünü artır

    // Varsayılan fontları temizle ve yenisini yükle
    io.Fonts->Clear();
    io.Fonts->AddFontFromFileTTF("monospace.medium.ttf", font_size);

    io.Fonts->Build();




    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        render_ui();

        // Canvas
        ImGui::Begin("Günes Pozisyonu");
        ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        ImVec2 center = ImVec2(canvas_p0.x + canvas_size.x * 0.5f, canvas_p0.y + canvas_size.y * 0.5f);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        //draw_list->AddCircle(center, 200, IM_COL32(255, 255, 255, 255)); // Gökyüzü çemberi

        ImVec2 place_1 = ImVec2((canvas_p0.x + canvas_size.x * 0.5f) - 30, canvas_size.y * 0.5f);
        ImVec2 place_2 = ImVec2((canvas_p0.x + canvas_size.x * 0.5f) + 30, canvas_size.y * 0.5f);
        draw_list->AddLine(place_1, place_2, 3, 10.0f);
        DrawSun(draw_list, center, 200); // Güneş'i çiz
        ImGui::End();

        // Çizimleri uygula
        ImGui::Render();
        glViewport(0, 0, 800, 600);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
