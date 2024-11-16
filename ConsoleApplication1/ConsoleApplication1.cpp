#include <windows.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>
// UWAGA!!! uzywaj klawiatury: 1,2,3,4
// Struktura
struct Circle {
    int x, y, radius;
    COLORREF color;
    std::atomic<bool> running;
    std::mutex mtx;
    std::condition_variable cv;

    // Konstruktor
    Circle(Circle&& other) noexcept
        : x(other.x), y(other.y), radius(other.radius), color(other.color), running(other.running.load()) {
    }

    // Operator przypisania
    Circle& operator=(Circle&& other) noexcept {
        if (this != &other) {
            x = other.x;
            y = other.y;
            radius = other.radius;
            color = other.color;
            running.store(other.running.load());
        }
        return *this;
    }

    // Konstruktor kopiujący i operator przypisania są usuwane
    Circle(const Circle&) = delete;
    Circle& operator=(const Circle&) = delete;

    // Konstruktor
    Circle(int x, int y, int radius, COLORREF color, bool running = false)
        : x(x), y(y), radius(radius), color(color), running(running) {
    }
};

std::vector<Circle> circles;
std::vector<std::thread> threads;
HWND hwnd;

// rysuje
void DrawCircle(HDC hdc, Circle& circle) {
    HBRUSH brush = CreateSolidBrush(circle.color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    Ellipse(hdc, circle.x - circle.radius, circle.y - circle.radius, circle.x + circle.radius, circle.y + circle.radius);
    SelectObject(hdc, oldBrush);
    DeleteObject(brush);
}

// animacja koła
void CircleThread(Circle& circle) {
    while (true) {
        std::unique_lock<std::mutex> lock(circle.mtx);
        circle.cv.wait(lock, [&]() { return circle.running.load(); });
        lock.unlock();

        // Zaktualizuj współrzędne koła (prosta animacja)
        circle.x = (circle.x + 5) % 400;

        // Odśwież okno
        InvalidateRect(hwnd, nullptr, TRUE);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

// Procedura okna
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        for (auto& circle : circles) {
            DrawCircle(hdc, circle);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wParam >= '1' && wParam <= '4') {
            int index = static_cast<int>(wParam - '1');
            if (index < circles.size()) {
                Circle& circle = circles[index];
                {
                    std::lock_guard<std::mutex> lock(circle.mtx);
                    circle.running = !circle.running;
                }
                circle.cv.notify_one();
            }
        }
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Główna funkcja programu
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {
    // Rejestracja klasy okna
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"CircleAnimationClass"; // Unicode string
    RegisterClass(&wc);

    // Tworzenie okna
    hwnd = CreateWindowEx(0, L"CircleAnimationClass", L"Wirujące Kółka", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 400,
        nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);

    // Dodajemy koła do listy
    circles.emplace_back(50, 100, 20, RGB(255, 0, 0), false);
    circles.emplace_back(150, 200, 30, RGB(0, 255, 0), false);
    circles.emplace_back(250, 100, 25, RGB(0, 0, 255), false);
    circles.emplace_back(250, 200, 35, RGB(255, 255, 0), false);

    // Tworzymy wątki
    for (auto& circle : circles) {
        threads.emplace_back(CircleThread, std::ref(circle));
    }

    // Pętla obsługi wiadomości
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Zatrzymujemy wątki
    for (auto& circle : circles) {
        circle.running = false;
        circle.cv.notify_one();
    }
    for (auto& thread : threads) {
        thread.join();
    }

    return 0;
}
