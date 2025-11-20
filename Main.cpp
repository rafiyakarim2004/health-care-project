// full_medical_app.cpp
#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
using namespace std;

// ----------------- Data Structures -----------------
struct Chamber {
    string address;
    string visitingHours; // e.g. "9AM-12PM"
};

struct Doctor {
    string name;
    string department;
    string education;
    string gender;
    float fee;
    string contact;
    float rating;
    vector<Chamber> chambers; // multiple chambers possible
    string district;
    float displayY, targetY;
};

struct TestFee {
    string testName;
    string hospitalName;
    int fee;
    string district;
    float displayY, targetY;
};

struct Medicine {
    string name;
    string description;
    string sideEffects;
    string suggestions;
    int pricePerPc;
};

struct Donor {
    string bloodGroup;
    string name;
    string number;
    string area;
    string district;
    float displayY, targetY;
};

struct Ambulance {
    string companyName;
    string driverName;
    string driverNumber;
    string area;
    string district;
    float displayY, targetY;
};

struct Popup {
    GLFWwindow* win;
    int type; // 0 = doctor, 1 = test, 2 = medicine, 3 = donor, 4 = ambulance
    int index; // index in appropriate vector
    float scrollOffset;
};

// ----------------- Global State -----------------
enum Section { HOME, DOCTORS, TESTS, MEDICINES, DONORS, AMBULANCES };
Section currentSection = HOME;

vector<Doctor> doctors;
vector<TestFee> tests;
vector<Medicine> medicines;
vector<Donor> donors;
vector<Ambulance> ambulances;
vector<Popup> popups;

// Scroll offsets for each section
float scrollDoctors = 0.0f;
float scrollTests = 0.0f;
float scrollMedicines = 0.0f;
float scrollDonors = 0.0f;
float scrollAmbulances = 0.0f;

// UI filter state (strings "All" or specific)
string filterDept = "All";
string filterGender = "All";
string filterDistrict = "All";

string filterTestName = "All";
string filterHospital = "All";
string filterTestDistrict = "All";

string filterBloodGroup = "All";
string filterDonorDistrict = "All";
string filterDonorArea = "All";

string filterAmbArea = "All";
string filterAmbDistrict = "All";

// action state
vector<bool> appointmentBooked; // per doctor
vector<bool> medicineBought; // per medicine
vector<bool> donorContacted; // per donor
vector<bool> ambulanceContacted; // per ambulance

// ----------------- File Save Helpers -----------------
void saveAppointmentToFile(const Doctor& d) {
    ofstream file("appointments.txt", ios::app);
    if (!file) return;

    file << "Doctor Name: " << d.name << "\n";
    file << "Department: " << d.department << "\n";
    file << "District: " << d.district << "\n";
    file << "Fee: " << d.fee << "\n";
    file << "Rating: " << d.rating << "\n";
    file << "Contact: " << d.contact << "\n";
    file << "------------------------------\n";
    file.close();
}

void saveMedicineToFile(const Medicine& m) {
    ofstream file("medicine_sales.txt", ios::app);
    if (!file) return;

    file << "Medicine Name: " << m.name << "\n";
    file << "Price Per Piece: " << m.pricePerPc << "\n";
    file << "Description: " << m.description << "\n";
    file << "Suggestions: " << m.suggestions << "\n";
    file << "------------------------------\n";
    file.close();
}

// ----------------- Scroll callbacks -----------------
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    float scrollSpeed = 30.0f;
    switch (currentSection) {
    case DOCTORS: scrollDoctors += yoffset * scrollSpeed; break;
    case TESTS: scrollTests += yoffset * scrollSpeed; break;
    case MEDICINES: scrollMedicines += yoffset * scrollSpeed; break;
    case DONORS: scrollDonors += yoffset * scrollSpeed; break;
    case AMBULANCES: scrollAmbulances += yoffset * scrollSpeed; break;
    default: break;
    }
}

void popupScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    for (auto& p : popups) {
        if (p.win == window) {
            p.scrollOffset += yoffset * 20.0f;
            break;
        }
    }
}

// ----------------- Utility Draw Functions -----------------
void drawText(float x, float y, const string& text, float r = 1, float g = 1, float b = 1) {
    glColor3f(r, g, b);
    char buffer[99999];
    int quads = stb_easy_font_print((float)x, (float)y, (char*)text.c_str(), nullptr, buffer, sizeof(buffer));
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 16, buffer);
    glDrawArrays(GL_QUADS, 0, quads * 4);
    glDisableClientState(GL_VERTEX_ARRAY);
}

bool drawButton(float x, float y, float w, float h, const string& label, double mx, double my, bool mouseClick) {
    bool hover = mx >= x && mx <= x + w && my >= y && my <= y + h;
    // button color - modern teal/cyan theme
    glColor3f(hover ? 0.2f : 0.15f, hover ? 0.7f : 0.55f, hover ? 0.75f : 0.65f);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y);
    glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
    drawText(x + 8, y + 8, label, 1, 1, 1);
    return hover && mouseClick;
}

GLFWwindow* createPopupWindow(int w, int h, const string& title) {
    GLFWwindow* pw = glfwCreateWindow(w, h, title.c_str(), nullptr, nullptr);
    if (!pw) return nullptr;
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    glfwSetWindowPos(pw, (mode->width - w) / 2 + 30, (mode->height - h) / 2 + 30);
    glfwSetScrollCallback(pw, popupScrollCallback);
    return pw;
}

void openPopup(int type, int index, const string& title, int w = 420, int h = 420) {
    // ensure only one popup per (type,index)
    for (auto& p : popups)
        if (p.type == type && p.index == index) return;
    GLFWwindow* pw = createPopupWindow(w, h, title);
    if (pw) popups.push_back({ pw, type, index, 0.0f });
}

// ----------------- Sample Data Initialization -----------------
void initSampleData() {
    doctors = {
        // Original doctors
        {"Dr. Ahsan", "Cardiologist", "MBBS, FCPS (Cardiology)", "Male", 80, "01711111111", 4.2f,
         { {"Heart Tower, Room 101", "9AM-12PM"}, {"City Clinic, Suite 5", "5PM-7PM"} }, "Dhaka"},
        {"Dr. Rafi", "Neurologist", "MBBS, MD (Neuro)", "Male", 90, "01722222222", 4.7f,
         { {"Neuro Center, Floor 2", "2PM-5PM"} }, "Chittagong"},
        {"Dr. Lima", "Pediatrician", "MBBS, DCH", "Female", 70, "01733333333", 4.5f,
         { {"Kids Care, Block B", "10AM-1PM"}, {"HealthPoint, Room 12", "3PM-4PM"} }, "Dhaka"},
        {"Dr. Rahman", "Surgeon", "MBBS, MS (Surgery)", "Male", 120, "01744444444", 4.9f,
         { {"Surgery Center, OT Wing", "4PM-8PM"} }, "Rajshahi"},

         // New doctors - Cardiologists
         {"Dr. Fatima", "Cardiologist", "MBBS, MD (Cardiology)", "Female", 85, "01755555555", 4.6f,
          { {"Heart Care Hospital, Room 205", "10AM-1PM"}, {"Cardiac Center, Floor 3", "4PM-6PM"} }, "Chittagong"},
         {"Dr. Karim", "Cardiologist", "MBBS, FCPS (Cardiology), FRCP", "Male", 95, "01766666666", 4.8f,
          { {"National Heart Foundation, Block A", "8AM-11AM"} }, "Dhaka"},

          // New doctors - Neurologists
          {"Dr. Nasrin", "Neurologist", "MBBS, FCPS (Neurology)", "Female", 88, "01777777777", 4.5f,
           { {"Brain & Spine Center, Room 301", "3PM-7PM"} }, "Dhaka"},
          {"Dr. Mahmud", "Neurologist", "MBBS, MD (Neurology), PhD", "Male", 100, "01788888888", 4.9f,
           { {"Neuro Hospital, Floor 4", "9AM-12PM"}, {"City Neuro Clinic, Suite 8", "2PM-5PM"} }, "Rajshahi"},

           // New doctors - Pediatricians
           {"Dr. Shabnam", "Pediatrician", "MBBS, FCPS (Pediatrics)", "Female", 75, "01799999999", 4.4f,
            { {"Children Hospital, Ward 2", "8AM-2PM"} }, "Chittagong"},
           {"Dr. Anik", "Pediatrician", "MBBS, MD (Pediatrics)", "Male", 72, "01800000000", 4.3f,
            { {"Kids Health Center, Room 15", "11AM-3PM"}, {"Pediatric Care, Floor 1", "5PM-8PM"} }, "Dhaka"},
           {"Dr. Rukhsana", "Pediatrician", "MBBS, DCH, FCPS", "Female", 78, "01811111110", 4.6f,
            { {"Baby Care Clinic, Block C", "9AM-1PM"} }, "Rajshahi"},

            // New doctors - Surgeons
            {"Dr. Hasan", "Surgeon", "MBBS, FCPS (Surgery)", "Male", 110, "01822222220", 4.7f,
             { {"General Surgery Unit, Floor 5", "6AM-10AM"}, {"Emergency OT, Wing B", "6PM-10PM"} }, "Dhaka"},
            {"Dr. Nadia", "Surgeon", "MBBS, MS (General Surgery)", "Female", 105, "01833333330", 4.5f,
             { {"Surgical Center, Room 401", "2PM-6PM"} }, "Chittagong"},
            {"Dr. Iqbal", "Surgeon", "MBBS, FRCS", "Male", 130, "01844444440", 4.9f,
             { {"Advanced Surgery Center, OT Complex", "7AM-11AM"} }, "Rajshahi"},

             // Additional doctors - Mixed departments
             {"Dr. Tasneem", "Cardiologist", "MBBS, DCard, MD", "Female", 82, "01855555550", 4.4f,
              { {"Heart Clinic, Floor 2", "1PM-5PM"} }, "Rajshahi"},
             {"Dr. Bashir", "Neurologist", "MBBS, FCPS (Neuro)", "Male", 92, "01866666660", 4.6f,
              { {"Neuro Care Center, Room 102", "10AM-2PM"} }, "Chittagong"},
             {"Dr. Sadia", "Pediatrician", "MBBS, MCPS, DCH", "Female", 68, "01877777770", 4.2f,
              { {"Child Health Clinic, Block D", "8AM-12PM"}, {"Kids Medical Center, Suite 3", "3PM-6PM"} }, "Dhaka"},
             {"Dr. Rafiq", "Surgeon", "MBBS, MS (Surgery), FACS", "Male", 125, "01888888880", 4.8f,
              { {"Surgical Hospital, OT Floor", "5AM-9AM"} }, "Chittagong"}
    };

    tests = {
        {"ECG", "City Hospital", 300, "Dhaka"},
        {"MRI", "Neuro Center", 1500, "Chittagong"},
        {"Blood Test", "HealthLab", 400, "Dhaka"},
        {"CT Scan", "City Hospital", 2000, "Chittagong"}
    };

    medicines = {
        {"Paracetamol", "Pain reliever & fever reducer.", "Nausea in some patients", "Take after food", 20},
        {"Amoxicillin", "Antibiotic for bacterial infections.", "Diarrhea, allergic reactions", "Complete full course", 50},
        {"Ibuprofen", "NSAID - reduces inflammation & pain.", "Stomach upset", "Avoid if stomach ulcer", 30},
        {"Cough Syrup", "Relieves cough.", "Drowsiness", "Don't operate machinery", 60},
        {"Vitamin C", "Supplement for immunity.", "Stomach upset in high dose", "Take with water", 25},
        {"Antacid", "Neutralizes stomach acid.", "Constipation or diarrhea", "Use as needed", 15},
        {"Metformin", "Diabetes medication.", "GI upset", "Take with meals", 40},
        {"Aspirin", "Pain reliever & blood thinner.", "Bleeding risk", "Avoid before surgery", 35},
        {"Azithromycin", "Antibiotic.", "Nausea", "Finish the dose", 80},
        {"Eye Drops", "Lubricating eye drops.", "Temporary blurred vision", "Don't touch dropper tip", 45}
    };

    donors = {
        // Original donors
        {"A+", "Rafi Ahmed", "01810000001", "Banani", "Dhaka"},
        {"O-", "Salma Khatun", "01810000002", "Gulshan", "Dhaka"},
        {"B+", "Imran Khan", "01810000003", "Pahartali", "Chittagong"},

        // New donors - Dhaka
        {"A+", "Kamal Hossain", "01810000004", "Dhanmondi", "Dhaka"},
        {"A-", "Nusrat Jahan", "01810000005", "Mirpur", "Dhaka"},
        {"B+", "Shakib Rahman", "01810000006", "Uttara", "Dhaka"},
        {"B-", "Faria Islam", "01810000007", "Mohakhali", "Dhaka"},
        {"O+", "Mahmud Ali", "01810000008", "Badda", "Dhaka"},
        {"O-", "Ayesha Siddique", "01810000009", "Banani", "Dhaka"},
        {"AB+", "Tanvir Ahmed", "01810000010", "Gulshan", "Dhaka"},
        {"AB-", "Rahat Khan", "01810000011", "Tejgaon", "Dhaka"},

        // New donors - Chittagong
        {"A+", "Saif Hassan", "01810000012", "Agrabad", "Chittagong"},
        {"A-", "Zara Tabassum", "01810000013", "Nasirabad", "Chittagong"},
        {"B+", "Fahim Chowdhury", "01810000014", "Patenga", "Chittagong"},
        {"B-", "Lamia Sultana", "01810000015", "Kotwali", "Chittagong"},
        {"O+", "Rahim Uddin", "01810000016", "Pahartali", "Chittagong"},
        {"O-", "Tasnim Akhter", "01810000017", "Halishahar", "Chittagong"},
        {"AB+", "Farhan Morshed", "01810000018", "Agrabad", "Chittagong"},

        // New donors - Rajshahi
        {"A+", "Sajid Hasan", "01810000019", "Shaheb Bazar", "Rajshahi"},
        {"A-", "Mim Akter", "01810000020", "Boalia", "Rajshahi"},
        {"B+", "Rifat Islam", "01810000021", "Rajpara", "Rajshahi"},
        {"B-", "Naima Khan", "01810000022", "Uposhohor", "Rajshahi"},
        {"O+", "Asif Mahmud", "01810000023", "Shaheb Bazar", "Rajshahi"},
        {"O-", "Sabrina Ahmed", "01810000024", "Kazla", "Rajshahi"},
        {"AB+", "Touhid Rahman", "01810000025", "Boalia", "Rajshahi"},
        {"AB-", "Priya Saha", "01810000026", "Rajpara", "Rajshahi"},

        // Additional mixed donors
        {"A+", "Hasan Mahmud", "01810000027", "Mohammadpur", "Dhaka"},
        {"O+", "Sadia Afrin", "01810000028", "Khulshi", "Chittagong"},
        {"B+", "Nafis Ahmed", "01810000029", "Motihar", "Rajshahi"},
        {"AB+", "Tisha Rahman", "01810000030", "Rampura", "Dhaka"}
    };

    ambulances = {
        // Original ambulances
        {"RapidCare Ambulance", "Karim", "01910000001", "Gulshan", "Dhaka"},
        {"City Ambulance", "Rahim", "01910000002", "Pahartali", "Chittagong"},

        // New ambulances - Dhaka
        {"LifeLine Emergency", "Salman Khan", "01910000003", "Dhanmondi", "Dhaka"},
        {"QuickAid Ambulance", "Rafiqul Islam", "01910000004", "Mirpur", "Dhaka"},
        {"MedExpress Services", "Jahangir Alam", "01910000005", "Uttara", "Dhaka"},
        {"Emergency Care 24/7", "Masum Billah", "01910000006", "Mohakhali", "Dhaka"},
        {"FastTrack Ambulance", "Sumon Ahmed", "01910000007", "Banani", "Dhaka"},
        {"HealthFirst Transport", "Nasir Uddin", "01910000008", "Badda", "Dhaka"},
        {"Rapid Response Team", "Kamrul Hasan", "01910000009", "Tejgaon", "Dhaka"},

        // New ambulances - Chittagong
        {"SeaCity Ambulance", "Abdul Kader", "01910000010", "Agrabad", "Chittagong"},
        {"Port Emergency Services", "Mizanur Rahman", "01910000011", "Nasirabad", "Chittagong"},
        {"Coastal Care Ambulance", "Shakil Ahmed", "01910000012", "Patenga", "Chittagong"},
        {"ChittagongCare Transport", "Jamal Hossain", "01910000013", "Kotwali", "Chittagong"},
        {"Bay Ambulance Service", "Rashed Khan", "01910000014", "Halishahar", "Chittagong"},
        {"QuickHelp Emergency", "Monir Uddin", "01910000015", "Khulshi", "Chittagong"},

        // New ambulances - Rajshahi
        {"Rajshahi Emergency Care", "Alamgir Hossain", "01910000016", "Shaheb Bazar", "Rajshahi"},
        {"NorthCare Ambulance", "Babul Mia", "01910000017", "Boalia", "Rajshahi"},
        {"Swift Medical Transport", "Harun Rashid", "01910000018", "Rajpara", "Rajshahi"},
        {"Reliable Ambulance", "Selim Reza", "01910000019", "Uposhohor", "Rajshahi"},
        {"Express Emergency", "Mostafa Kamal", "01910000020", "Kazla", "Rajshahi"},
        {"HealthRide Services", "Aziz Rahman", "01910000021", "Motihar", "Rajshahi"},

        // Additional mixed ambulances
        {"Metro Ambulance", "Sadiq Ali", "01910000022", "Rampura", "Dhaka"},
        {"AllDay Emergency", "Habib Ullah", "01910000023", "Lalkhan Bazar", "Chittagong"},
        {"CityWide Ambulance", "Tariq Hasan", "01910000024", "Upashahar", "Rajshahi"}
    };

    // prepare action state sizes
    appointmentBooked.assign(doctors.size(), false);
    medicineBought.assign(medicines.size(), false);
    donorContacted.assign(donors.size(), false);
    ambulanceContacted.assign(ambulances.size(), false);

    // init display positions
    float sy = 160;
    for (int i = 0; i < doctors.size(); ++i) {
        doctors[i].displayY = sy + i * 60;
        doctors[i].targetY = doctors[i].displayY;
    }
    sy = 220;
    for (int i = 0; i < tests.size(); ++i) { tests[i].displayY = sy + i * 60; tests[i].targetY = tests[i].displayY; }
    sy = 200;
    for (int i = 0; i < medicines.size(); ++i) { /* medicines don't animate as list */ }
    sy = 200;
    for (int i = 0; i < donors.size(); ++i) { donors[i].displayY = sy + i * 60; donors[i].targetY = donors[i].displayY; }
    sy = 200;
    for (int i = 0; i < ambulances.size(); ++i) { ambulances[i].displayY = sy + i * 60; ambulances[i].targetY = ambulances[i].displayY; }
}

// ----------------- UI Helpers -----------------
void drawHome(double mx, double my, bool click) {
    drawText(380, 60, "Medical Info App", 0.4f, 0.9f, 1.0f);
    float bx = 300, by = 150, bw = 300, bh = 60, gap = 80;
    if (drawButton(bx, by, bw, bh, "1. Doctors Dictionary", mx, my, click)) currentSection = DOCTORS;
    if (drawButton(bx, by + gap, bw, bh, "2. Test Fees Comparison", mx, my, click)) currentSection = TESTS;
    if (drawButton(bx, by + gap * 2, bw, bh, "3. Medicine Shop", mx, my, click)) currentSection = MEDICINES;
    if (drawButton(bx, by + gap * 3, bw, bh, "4. Blood Donor Bank", mx, my, click)) currentSection = DONORS;
    if (drawButton(bx, by + gap * 4, bw, bh, "5. Ambulance Self Service", mx, my, click)) currentSection = AMBULANCES;
}

void drawBackHomeButton(double mx, double my, bool click) {
    if (drawButton(20, 20, 120, 30, "Back to Home", mx, my, click)) {
        currentSection = HOME;
        // reset filters and scroll
        filterDept = filterGender = filterDistrict = "All";
        filterTestName = filterHospital = filterTestDistrict = "All";
        filterBloodGroup = filterDonorDistrict = filterDonorArea = "All";
        filterAmbArea = filterAmbDistrict = "All";
        scrollDoctors = scrollTests = scrollMedicines = scrollDonors = scrollAmbulances = 0.0f;
    }
}

// ----------------- Section Renderers -----------------
void renderDoctors(double mx, double my, bool click) {
    drawText(400, 60, "Doctors Dictionary", 0.3f, 0.9f, 1.0f);
    drawText(820, 60, "Use scroll to navigate", 0.7f, 0.7f, 0.7f);

    // Filters: Dept, Gender, District
    vector<string> depts = { "All", "Cardiologist", "Neurologist", "Pediatrician", "Surgeon" };
    float bx = 20;
    for (auto& d : depts) {
        if (drawButton(bx, 100, 140, 30, d, mx, my, click)) filterDept = d;
        bx += 150;
    }
    bx = 20;
    vector<string> genders = { "All", "Male", "Female" };
    for (auto& g : genders) {
        if (drawButton(bx, 140, 120, 28, g, mx, my, click)) filterGender = g;
        bx += 130;
    }
    vector<string> districts = { "All", "Dhaka", "Chittagong", "Rajshahi" };
    bx = 20;
    for (auto& di : districts) {
        if (drawButton(bx, 180, 120, 28, di, mx, my, click)) filterDistrict = di;
        bx += 130;
    }

    // Clamp scroll
    if (scrollDoctors > 0) scrollDoctors = 0;

    // List with scroll
    float listY = 240 + scrollDoctors;
    for (int i = 0; i < doctors.size(); ++i) {
        Doctor& doc = doctors[i];
        if ((filterDept != "All" && doc.department != filterDept) ||
            (filterGender != "All" && doc.gender != filterGender) ||
            (filterDistrict != "All" && doc.district != filterDistrict))
            continue;

        float barY = listY;
        float barX = 40, barH = 48, barW = 720;

        // Only render if visible
        if (barY + barH < 220 || barY > 720) {
            listY += 70;
            continue;
        }

        // color by department
        if (doc.department == "Cardiologist") glColor3f(0.85f, 0.3f, 0.4f);
        else if (doc.department == "Neurologist") glColor3f(0.3f, 0.75f, 0.5f);
        else if (doc.department == "Pediatrician") glColor3f(0.9f, 0.7f, 0.3f);
        else glColor3f(0.4f, 0.5f, 0.85f);

        glBegin(GL_QUADS);
        glVertex2f(barX, barY); glVertex2f(barX + barW, barY);
        glVertex2f(barX + barW, barY + barH); glVertex2f(barX, barY + barH);
        glEnd();

        ostringstream oss; oss << fixed << setprecision(1) << doc.fee;
        string summary = doc.name + " (" + doc.department + ")  Fee: Tk " + oss.str() + "  Rating: " + to_string(doc.rating) + "  Dist: " + doc.district;
        drawText(barX + 10, barY + 14, summary, 1, 1, 1);

        // Book button on the right of each doctor entry
        if (drawButton(barX + barW - 130, barY + 10, 100, 28, "Book", mx, my, click)) {
            appointmentBooked[i] = true;
            saveAppointmentToFile(doc);   // <-- SAVE TO FILE when book from list
        }
        if (appointmentBooked[i]) drawText(barX + barW - 230, barY + 16, "Booked", 0.4f, 1.0f, 0.4f);

        if (click && mx >= barX && mx <= barX + barW && my >= barY && my <= barY + barH) {
            openPopup(0, i, doc.name + " - Details");
        }
        listY += 70;
    }
}

void renderTests(double mx, double my, bool click) {
    drawText(350, 60, "Test Fees Comparison", 0.3f, 0.9f, 1.0f);
    drawText(820, 60, "Use scroll to navigate", 0.7f, 0.7f, 0.7f);

    // Filters
    vector<string> tnames = { "All", "ECG", "MRI", "Blood Test", "CT Scan" };
    float bx = 20;
    for (auto& t : tnames) {
        if (drawButton(bx, 100, 120, 28, t, mx, my, click)) filterTestName = t;
        bx += 130;
    }
    bx = 20;
    vector<string> hospitals = { "All", "City Hospital", "Neuro Center", "HealthLab" };
    for (auto& h : hospitals) {
        if (drawButton(bx, 140, 150, 28, h, mx, my, click)) filterHospital = h;
        bx += 160;
    }
    bx = 20;
    vector<string> districts = { "All", "Dhaka", "Chittagong" };
    for (auto& di : districts) {
        if (drawButton(bx, 180, 120, 28, di, mx, my, click)) filterTestDistrict = di;
        bx += 130;
    }

    if (scrollTests > 0) scrollTests = 0;

    float listY = 240 + scrollTests;
    for (int i = 0; i < tests.size(); ++i) {
        TestFee& t = tests[i];
        if ((filterTestName != "All" && t.testName != filterTestName) ||
            (filterHospital != "All" && t.hospitalName != filterHospital) ||
            (filterTestDistrict != "All" && t.district != filterTestDistrict))
            continue;

        float barY = listY;
        float barX = 40, barH = 48;
        float maxW = 700;
        float w = 200 + (t.fee * 0.2f);
        if (w > maxW) w = maxW;

        if (barY + barH < 220 || barY > 720) {
            listY += 70;
            continue;
        }

        glColor3f(0.5f, 0.45f, 0.75f);
        glBegin(GL_QUADS);
        glVertex2f(barX, barY); glVertex2f(barX + w, barY);
        glVertex2f(barX + w, barY + barH); glVertex2f(barX, barY + barH);
        glEnd();

        drawText(barX + 10, barY + 14, t.testName + " @ " + t.hospitalName + "  Tk " + to_string(t.fee) + "  Dist: " + t.district, 1, 1, 1);

        if (click && mx >= barX && mx <= barX + w && my >= barY && my <= barY + barH) {
            openPopup(1, i, t.testName + " - Details");
        }
        listY += 70;
    }
}

void renderMedicines(double mx, double my, bool click) {
    drawText(400, 60, "Medicine Shop", 0.3f, 0.9f, 1.0f);
    drawText(820, 60, "Use scroll to navigate", 0.7f, 0.7f, 0.7f);

    if (scrollMedicines > 0) scrollMedicines = 0;

    float y = 120 + scrollMedicines;
    for (int i = 0; i < medicines.size(); ++i) {
        Medicine& m = medicines[i];

        if (y + 48 < 100 || y > 720) {
            y += 80;
            continue;
        }

        glColor3f(0.3f, 0.5f, 0.75f);
        glBegin(GL_QUADS);
        glVertex2f(20, y - 8); glVertex2f(520, y - 8);
        glVertex2f(520, y + 48); glVertex2f(20, y + 48);
        glEnd();

        drawText(30, y, m.name + " - Tk " + to_string(m.pricePerPc), 1, 1, 1);
        drawText(30, y + 18, "Desc: " + m.description);
        drawText(30, y + 34, "Side effects: " + m.sideEffects + "  Suggestion: " + m.suggestions);

        if (drawButton(540, y - 8, 100, 40, "Buy", mx, my, click)) {
            medicineBought[i] = true;
            saveMedicineToFile(m); // <-- SAVE TO FILE when buying from list
        }

        if (medicineBought[i]) drawText(650, y + 8, "Done!", 0.4f, 1.0f, 0.4f);

        if (click && mx >= 20 && mx <= 520 && my >= y - 8 && my <= y + 48) {
            openPopup(2, i, m.name + " - Details");
        }
        y += 80;
    }
}

void renderDonors(double mx, double my, bool click) {
    drawText(380, 60, "Blood Donor Bank", 0.3f, 0.9f, 1.0f);
    drawText(820, 60, "Use scroll to navigate", 0.7f, 0.7f, 0.7f);

    // filters
    vector<string> groups = { "All", "A+", "A-", "B+", "B-", "O+", "O-", "AB+", "AB-" };
    float bx = 20;
    for (auto& g : groups) {
        if (drawButton(bx, 100, 90, 28, g, mx, my, click)) filterBloodGroup = g;
        bx += 100;
    }
    bx = 20;
    vector<string> districts = { "All", "Dhaka", "Chittagong", "Rajshahi" };
    for (auto& d : districts) {
        if (drawButton(bx, 140, 120, 28, d, mx, my, click)) filterDonorDistrict = d;
        bx += 130;
    }
    if (drawButton(bx, 140, 120, 28, "Area All", mx, my, click)) filterDonorArea = "All";

    if (scrollDonors > 0) scrollDonors = 0;

    float listY = 220 + scrollDonors;
    for (int i = 0; i < donors.size(); ++i) {
        Donor& D = donors[i];
        if ((filterBloodGroup != "All" && D.bloodGroup != filterBloodGroup) ||
            (filterDonorDistrict != "All" && D.district != filterDonorDistrict) ||
            (filterDonorArea != "All" && D.area != filterDonorArea))
            continue;

        float barY = listY;

        if (barY + 48 < 200 || barY > 720) {
            listY += 70;
            continue;
        }

        glColor3f(0.75f, 0.35f, 0.35f);
        glBegin(GL_QUADS);
        glVertex2f(20, barY); glVertex2f(760, barY);
        glVertex2f(760, barY + 48); glVertex2f(20, barY + 48);
        glEnd();

        drawText(30, barY + 12, D.bloodGroup + "  " + D.name + "  " + D.number + "  Area:" + D.area + "  Dist:" + D.district);

        if (click && mx >= 20 && mx <= 760 && my >= barY && my <= barY + 48)
            openPopup(3, i, D.name + " - Details");

        listY += 70;
    }
}

void renderAmbulances(double mx, double my, bool click) {
    drawText(350, 60, "Ambulance Self Service", 0.3f, 0.9f, 1.0f);
    drawText(820, 60, "Use scroll to navigate", 0.7f, 0.7f, 0.7f);

    // filters
    vector<string> districts = { "All", "Dhaka", "Chittagong", "Rajshahi" };
    float bx = 20;
    for (auto& d : districts) {
        if (drawButton(bx, 100, 120, 28, d, mx, my, click)) filterAmbDistrict = d;
        bx += 130;
    }
    if (drawButton(bx, 100, 120, 28, "Area All", mx, my, click)) filterAmbArea = "All";

    if (scrollAmbulances > 0) scrollAmbulances = 0;

    float listY = 180 + scrollAmbulances;
    for (int i = 0; i < ambulances.size(); ++i) {
        Ambulance& a = ambulances[i];
        if ((filterAmbDistrict != "All" && a.district != filterAmbDistrict) ||
            (filterAmbArea != "All" && a.area != filterAmbArea))
            continue;

        float barY = listY;

        if (barY + 48 < 160 || barY > 720) {
            listY += 70;
            continue;
        }

        glColor3f(0.35f, 0.6f, 0.7f);
        glBegin(GL_QUADS);
        glVertex2f(20, barY); glVertex2f(760, barY);
        glVertex2f(760, barY + 48); glVertex2f(20, barY + 48);
        glEnd();

        drawText(30, barY + 12, a.companyName + " Driver: " + a.driverName + "  " + a.driverNumber + "  Area:" + a.area + "  Dist:" + a.district);

        if (click && mx >= 20 && mx <= 760 && my >= barY && my <= barY + 48)
            openPopup(4, i, a.companyName + " - Details");

        listY += 70;
    }
}

// ----------------- Popup Render -----------------
void renderPopups() {
    for (int p = popups.size() - 1; p >= 0; --p) {
        Popup& pop = popups[p];
        if (glfwWindowShouldClose(pop.win)) {
            glfwDestroyWindow(pop.win);
            popups.erase(popups.begin() + p);
            continue;
        }

        glfwMakeContextCurrent(pop.win);
        int pw, ph;
        glfwGetFramebufferSize(pop.win, &pw, &ph);
        glViewport(0, 0, pw, ph);
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        glOrtho(0, pw, ph, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        glClearColor(0.08f, 0.12f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        double pmx, pmy;
        glfwGetCursorPos(pop.win, &pmx, &pmy);
        bool click = glfwGetMouseButton(pop.win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

        // Clamp scroll
        if (pop.scrollOffset > 0) pop.scrollOffset = 0;

        // Doctor detail popup (type 0)
        if (pop.type == 0) {
            Doctor& d = doctors[pop.index];
            float y = 30 + pop.scrollOffset;

            drawText(20, y, "Doctor Details", 0.3f, 0.9f, 1.0f);
            y += 40;
            drawText(20, y, "Name: " + d.name);
            y += 30;
            drawText(20, y, "Department: " + d.department);
            y += 30;
            drawText(20, y, "Education: " + d.education);
            y += 30;
            drawText(20, y, "Gender: " + d.gender);
            y += 30;
            ostringstream oss; oss << fixed << setprecision(1) << d.fee;
            drawText(20, y, "Fee: Tk " + oss.str());
            y += 30;
            drawText(20, y, "Contact: " + d.contact);
            y += 30;
            drawText(20, y, "Rating: " + to_string(d.rating));
            y += 30;
            drawText(20, y, "District: " + d.district);
            y += 30;

            // chambers
            for (int c = 0; c < d.chambers.size(); ++c) {
                drawText(20, y, "Chamber: " + d.chambers[c].address + "  Hours: " + d.chambers[c].visitingHours);
                y += 24;
            }

            if (drawButton(260, ph - 60, 120, 30, "Appointment", pmx, pmy, click)) {
                appointmentBooked[pop.index] = true;
                saveAppointmentToFile(d); // <-- SAVE TO FILE when booking in popup
            }
            if (appointmentBooked[pop.index]) drawText(260, ph - 95, "Booked!", 0.4f, 1.0f, 0.4f);
            if (drawButton(pw - 90, ph - 40, 60, 28, "Close", pmx, pmy, click)) glfwSetWindowShouldClose(pop.win, true);
        }

        // Test popup (type 1)
        else if (pop.type == 1) {
            TestFee& t = tests[pop.index];
            float y = 30 + pop.scrollOffset;

            drawText(20, y, "Test Details", 0.3f, 0.9f, 1.0f);
            y += 40;
            drawText(20, y, "Test: " + t.testName);
            y += 30;
            drawText(20, y, "Hospital: " + t.hospitalName);
            y += 30;
            drawText(20, y, "Fee: Tk " + to_string(t.fee));
            y += 30;
            drawText(20, y, "District: " + t.district);

            if (drawButton(pw - 90, ph - 40, 60, 28, "Close", pmx, pmy, click)) glfwSetWindowShouldClose(pop.win, true);
        }

        // Medicine popup (type 2)
        else if (pop.type == 2) {
            Medicine& m = medicines[pop.index];
            float y = 30 + pop.scrollOffset;

            drawText(20, y, "Medicine Details", 0.3f, 0.9f, 1.0f);
            y += 40;
            drawText(20, y, "Name: " + m.name + "  Price: Tk " + to_string(m.pricePerPc));
            y += 30;
            drawText(20, y, "Description: " + m.description);
            y += 30;
            drawText(20, y, "Side effects: " + m.sideEffects);
            y += 30;
            drawText(20, y, "Suggestion: " + m.suggestions);

            if (drawButton(20, ph - 60, 100, 30, "Buy", pmx, pmy, click)) {
                medicineBought[pop.index] = true;
                saveMedicineToFile(m); // <-- SAVE TO FILE when buying in popup
            }
            if (medicineBought[pop.index]) drawText(140, ph - 55, "Done!", 0.4f, 1.0f, 0.4f);
            if (drawButton(pw - 90, ph - 40, 60, 28, "Close", pmx, pmy, click)) glfwSetWindowShouldClose(pop.win, true);
        }

        // Donor popup (type 3)
        else if (pop.type == 3) {
            Donor& D = donors[pop.index];
            float y = 30 + pop.scrollOffset;

            drawText(20, y, "Donor Details", 0.3f, 0.9f, 1.0f);
            y += 40;
            drawText(20, y, "Name: " + D.name);
            y += 30;
            drawText(20, y, "Blood Group: " + D.bloodGroup);
            y += 30;
            drawText(20, y, "Number: " + D.number);
            y += 30;
            drawText(20, y, "Area: " + D.area + "  District: " + D.district);

            if (drawButton(20, ph - 60, 120, 30, "Contact", pmx, pmy, click)) donorContacted[pop.index] = true;
            if (donorContacted[pop.index]) drawText(150, ph - 55, "Contacted!", 0.4f, 1.0f, 0.4f);
            if (drawButton(pw - 90, ph - 40, 60, 28, "Close", pmx, pmy, click)) glfwSetWindowShouldClose(pop.win, true);
        }

        // Ambulance popup (type 4)
        else if (pop.type == 4) {
            Ambulance& a = ambulances[pop.index];
            float y = 30 + pop.scrollOffset;

            drawText(20, y, "Ambulance Details", 0.3f, 0.9f, 1.0f);
            y += 40;
            drawText(20, y, "Company: " + a.companyName);
            y += 30;
            drawText(20, y, "Driver: " + a.driverName);
            y += 30;
            drawText(20, y, "Driver Number: " + a.driverNumber);
            y += 30;
            drawText(20, y, "Area: " + a.area + "  District: " + a.district);

            if (drawButton(20, ph - 60, 140, 30, "Contact Driver", pmx, pmy, click)) ambulanceContacted[pop.index] = true;
            if (ambulanceContacted[pop.index]) drawText(170, ph - 55, "Contacted!", 0.4f, 1.0f, 0.4f);
            if (drawButton(pw - 90, ph - 40, 60, 28, "Close", pmx, pmy, click)) glfwSetWindowShouldClose(pop.win, true);
        }

        glfwSwapBuffers(pop.win);
    }
}

// ----------------- Main -----------------
int main() {
    if (!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(1000, 720, "Medical Info App", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetScrollCallback(window, scrollCallback);

    initSampleData();

    bool mouseWasDown = false;

    while (!glfwWindowShouldClose(window)) {
        glfwMakeContextCurrent(window);
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        glOrtho(0, width, height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();

        // Dark green background as requested
        glClearColor(0.06f, 0.18f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        double mx_d, my_d;
        glfwGetCursorPos(window, &mx_d, &my_d);
        // Convert to same coordinate system (y down)
        double mx = mx_d;
        double my = my_d;

        bool mouseDownNow = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        bool click = mouseDownNow && !mouseWasDown;

        // draw header
        drawText(20, 30, "Medical Info System", 0.4f, 0.8f, 1.0f);

        // draw Back or Home view
        if (currentSection == HOME) {
            drawHome(mx, my, click);
        }
        else {
            drawBackHomeButton(mx, my, click);

            switch (currentSection) {
            case DOCTORS: renderDoctors(mx, my, click); break;
            case TESTS: renderTests(mx, my, click); break;
            case MEDICINES: renderMedicines(mx, my, click); break;
            case DONORS: renderDonors(mx, my, click); break;
            case AMBULANCES: renderAmbulances(mx, my, click); break;
            default: break;
            }
        }

        // popups (their own windows)
        renderPopups();

        glfwSwapBuffers(window);
        glfwPollEvents();
        mouseWasDown = mouseDownNow;
    }

    for (auto& p : popups) glfwDestroyWindow(p.win);
    glfwTerminate();
    return 0;
}
