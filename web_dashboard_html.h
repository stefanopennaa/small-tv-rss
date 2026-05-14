#pragma once
// =====================================================================
// File: web_dashboard_html.h
// Purpose: Main dashboard HTML/CSS/JS stored in PROGMEM
// Changelog (latest first):
//   - 2026.05.14: tickWiFiRetry fixed — forced reconnect now triggers when bootState == BOOT_WIFI even if WiFi appears connected (internet-only outage recovery).
//   - 2026.05.14: tickWiFiRetry now calls refreshAfterInternetRecovery directly after internet recovery, without depending on offlineScreenShown flag.
//   - 2026.05.14: Updated to use 'online' flag from /api, improved fetch timeout handling
//   - 2026.05.03: Header/comment structure normalized (format-only update)
//
// Features:
// - Responsive Bootstrap 5 design with dark/light mode support
// - Real-time weather and device status display with accurate online detection
// - Brightness control slider
// - RSS news feed viewer
// - Firmware update link (via ElegantOTA)
// - Auto-refresh every 60 seconds
// =====================================================================

const char INDEX_HTML[] PROGMEM = R"(
<!DOCTYPE html>
<html lang="it">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>MeteoClock</title>
    <script>
        // Keep Bootstrap theme in sync with system preference in real time.
        const colorSchemeQuery = window.matchMedia('(prefers-color-scheme: dark)');
        function applyTheme() {
            const theme = colorSchemeQuery.matches ? 'dark' : 'light';
            document.documentElement.setAttribute('data-bs-theme', theme);
        }
        applyTheme();
        if (typeof colorSchemeQuery.addEventListener === 'function') {
            colorSchemeQuery.addEventListener('change', applyTheme);
        } else if (typeof colorSchemeQuery.addListener === 'function') {
            colorSchemeQuery.addListener(applyTheme);
        }
    </script>
    <link rel="icon"
        href="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><text y='.9em' font-size='90'>☀️</text></svg>">
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Bebas+Neue&family=Oswald:wght@400;500;600&display=swap" rel="stylesheet">
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css" rel="stylesheet">
    <style>
        body {
            background: #f8f9fa;
            min-height: 100vh;
            font-family: 'Oswald', sans-serif;
            letter-spacing: 0.02em;
            transition: background-color 0.25s ease, color 0.25s ease;
        }

        [data-bs-theme="dark"] body {
            background: #131722;
            color: #e8ebf0;
        }

        .card {
            background: white;
            border: 1px solid rgba(0, 0, 0, 0.08);
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.05);
            transition: transform 0.3s ease, box-shadow 0.3s ease;
        }

        [data-bs-theme="dark"] .card {
            background: rgba(23, 28, 40, 0.92);
            border: 1px solid rgba(255, 255, 255, 0.12);
            box-shadow: 0 8px 28px rgba(0, 0, 0, 0.28);
        }

        .card:hover {
            transform: translateY(-5px);
            box-shadow: 0 8px 24px rgba(0, 0, 0, 0.1);
        }

        .metric-card {
            position: relative;
            overflow: hidden;
        }

        .metric-card::before {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            right: 0;
            height: 4px;
            background: linear-gradient(90deg, #FFD93D, #FF6B35);
        }

        .header {
            background: white;
            border-radius: 1rem;
            padding: 1.5rem;
            margin-bottom: 2rem;
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.05);
            border: 1px solid rgba(0, 0, 0, 0.08);
        }

        [data-bs-theme="dark"] .header {
            background: rgba(23, 28, 40, 0.9);
            border: 1px solid rgba(255, 255, 255, 0.12);
            box-shadow: 0 10px 30px rgba(0, 0, 0, 0.25);
        }

        .header h1 {
            margin: 0;
            font-family: 'Bebas Neue', sans-serif;
            font-weight: 700;
            letter-spacing: 0.05em;
            background: linear-gradient(135deg, #FFD93D 0%, #FF6B35 100%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
        }

        .metric-value {
            font-family: 'Bebas Neue', sans-serif;
            font-size: clamp(1.5rem, 4vw, 2.5rem);
            font-weight: 700;
            letter-spacing: 0.05em;
            margin: 0.5rem 0;
        }

        .metric-label {
            font-family: 'Oswald', sans-serif;
            font-size: 0.875rem;
            text-transform: uppercase;
            letter-spacing: 0.08em;
            opacity: 0.7;
            font-weight: 600;
        }

        .section-title,
        .detail-value,
        .footer-text {
            font-family: 'Oswald', sans-serif;
        }

        .section-title {
            font-weight: 500;
            letter-spacing: 0.03em;
        }

        .detail-value {
            font-weight: 500;
        }

        .btn-custom {
            font-family: 'Oswald', sans-serif;
            background: linear-gradient(135deg, #FFD93D 0%, #FF6B35 100%);
            border: none;
            color: white;
            font-weight: 600;
            transition: all 0.3s ease;
        }

        .news-list,
        .status-badge,
        .form-label,
        .form-text,
        .card-text,
        .text-muted,
        p,
        li,
        a,
        button,
        input,
        label,
        span {
            font-family: 'Oswald', sans-serif;
        }

        .btn-custom:hover {
            transform: translateY(-2px);
            box-shadow: 0 8px 20px rgba(255, 107, 53, 0.4);
            color: white;
        }

        .news-list {
            max-height: 300px;
            overflow-y: auto;
        }

        .news-list::-webkit-scrollbar {
            width: 8px;
        }

        .news-list::-webkit-scrollbar-track {
            background: rgba(0, 0, 0, 0.05);
            border-radius: 10px;
        }

        [data-bs-theme="dark"] .news-list::-webkit-scrollbar-track {
            background: rgba(255, 255, 255, 0.08);
        }

        .news-list::-webkit-scrollbar-thumb {
            background: rgba(255, 217, 61, 0.5);
            border-radius: 10px;
        }

        [data-bs-theme="dark"] .text-muted,
        [data-bs-theme="dark"] .form-text,
        [data-bs-theme="dark"] .metric-label {
            color: rgba(232, 235, 240, 0.72) !important;
        }

        .news-list a {
            color: #FF6B35;
            text-decoration: none;
            transition: color 0.3s ease;
        }

        .news-list a:hover {
            color: #FFD93D;
            text-decoration: underline;
        }

        .form-range::-webkit-slider-thumb {
            background: linear-gradient(135deg, #FFD93D 0%, #FF6B35 100%);
        }

        .form-range::-moz-range-thumb {
            background: linear-gradient(135deg, #FFD93D 0%, #FF6B35 100%);
        }

        .loading {
            display: inline-block;
            animation: pulse 1.5s ease-in-out infinite;
        }

        @keyframes pulse {

            0%,
            100% {
                opacity: 1;
            }

            50% {
                opacity: 0.5;
            }
        }

        .status-badge {
            display: inline-block;
            padding: 0.25rem 0.75rem;
            border-radius: 2rem;
            font-size: 0.75rem;
            font-weight: 600;
            background: rgba(255, 217, 61, 0.2);
            color: #FF6B35;
        }

        @media (max-width: 768px) {
            .container {
                padding-left: 1rem;
                padding-right: 1rem;
            }

            .header {
                padding: 1rem;
            }
        }
    </style>
</head>

<body>
    <div class="container py-4">
        <!-- Header -->
        <div class="header">
            <div class="d-flex justify-content-between align-items-center flex-wrap gap-2">
                <h1 class="mb-0">☀️ MeteoClock</h1>
                <span class="status-badge" id="status">● Online</span>
            </div>
        </div>

        <div class="row g-3 g-md-4">
            <!-- Left column: weather data -->
            <div class="col-12 col-lg-4">
                <div class="d-flex flex-column gap-3">
                    <div class="card metric-card text-center p-4 rounded-4">
                        <div class="metric-label">Temperatura</div>
                        <div class="metric-value text-warning" id="temp">
                            <span class="loading">--</span>
                        </div>
                    </div>
                    <div class="card metric-card text-center p-4 rounded-4">
                        <div class="metric-label">Umidità</div>
                        <div class="metric-value text-info" id="hum">
                            <span class="loading">--</span>
                        </div>
                    </div>
                    <div class="card metric-card text-center p-4 rounded-4">
                        <div class="metric-label">Meteo</div>
                        <div class="detail-value fs-5 fw-bold mt-2" id="desc">
                            <span class="loading">--</span>
                        </div>
                    </div>
                    <div class="card metric-card text-center p-4 rounded-4">
                        <div class="metric-label">Indirizzo IP</div>
                        <div class="detail-value fs-6 fw-bold text-success mt-2" id="ip">
                            <span class="loading">--</span>
                        </div>
                    </div>
                </div>
            </div>

            <!-- Right column: actions and settings -->
            <div class="col-12 col-lg-8">
                <div class="d-flex flex-column gap-3">
                    <!-- Actions -->
                    <div class="card p-4 rounded-4">
                        <h5 class="section-title mb-3 d-flex align-items-center gap-2">
                            <span>⚙️</span>
                            <span>Azioni</span>
                        </h5>
                        <div class="d-grid gap-2">
                            <a 
                                href="/gtt" 
                                class="btn btn-custom btn-lg rounded-4"
                                role="button"
                                aria-label="Vai alla pagina GTT trasporti"
                            >
                                🚌 GTT Trasporti
                            </a>
                            <a 
                                href="/update" 
                                class="btn btn-custom btn-lg rounded-4"
                                role="button"
                                aria-label="Vai alla pagina di aggiornamento firmware"
                            >
                                🔄 Aggiorna Firmware
                            </a>
                        </div>
                    </div>

                    <!-- Brightness -->
                    <div class="card p-4 rounded-4">
                        <h5 class="section-title mb-3 d-flex align-items-center gap-2">
                            <span>💡</span>
                            <span>Luminosità</span>
                        </h5>
                        <label for="brightness" class="form-label visually-hidden">
                            Controllo luminosità schermo
                        </label>
                        <div class="d-flex align-items-center gap-3">
                            <span style="font-size: 1.25rem;" aria-hidden="true">🌑</span>
                            <input 
                                type="range" 
                                class="form-range flex-grow-1" 
                                min="0" 
                                max="255" 
                                id="brightness"
                                aria-label="Luminosità da 0 a 255"
                                aria-valuemin="0"
                                aria-valuemax="255"
                                aria-valuenow="0"
                                aria-describedby="brightness-help"
                            >
                            <span style="font-size: 1.25rem;" aria-hidden="true">🌕</span>
                        </div>
                        <div class="text-center mt-3">
                            <span class="status-badge" id="brightness-label" role="status" aria-live="polite" aria-atomic="true">--</span>
                            <small id="brightness-help" class="d-block text-muted mt-2 visually-hidden">
                                Usa le frecce per regolare la luminosità
                            </small>
                        </div>
                    </div>

                    <!-- News -->
                    <div class="card p-4 rounded-4">
                        <h5 class="section-title mb-3 d-flex align-items-center gap-2">
                            <span aria-hidden="true">📰</span>
                            <span>Notizie</span>
                        </h5>
                        <div 
                            id="news" 
                            class="news-list" 
                            role="region" 
                            aria-label="Feed notizie ANSA"
                            aria-live="polite"
                        >
                            <div class="loading" role="status">
                                <span>Caricamento notizie...</span>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <!-- Footer -->
        <footer class="text-center mt-4">
            <p class="footer-text text-muted small mb-0">
                Ultimo aggiornamento: 
                <time id="last-update" datetime="">--</time>
            </p>
        </footer>
    </div>

    <script>
        // =====================================================
        // Configuration Constants
        // =====================================================
        const CONFIG = {
            FETCH_TIMEOUT_MS: 8000,
            FETCH_MAX_RETRIES: 3,
            AUTO_REFRESH_INTERVAL_MS: 60000,
            BRIGHTNESS_DEBOUNCE_MS: 500,
            FEEDBACK_DURATION_MS: 1500
        };

        // =====================================================
        // Utility Functions
        // =====================================================
        
        /**
         * Fetch with automatic timeout and retry logic
         * @param {string} url - URL to fetch
         * @param {number} maxRetries - Maximum retry attempts
         * @param {number} timeout - Request timeout in milliseconds
         * @returns {Promise<any>} Parsed JSON response
         */
        const fetchWithRetry = async (url, maxRetries = CONFIG.FETCH_MAX_RETRIES, timeout = CONFIG.FETCH_TIMEOUT_MS) => {
            for (let attempt = 0; attempt < maxRetries; attempt++) {
                try {
                    const controller = new AbortController();
                    const timeoutId = setTimeout(() => controller.abort(), timeout);
                    
                    const response = await fetch(url, { signal: controller.signal });
                    clearTimeout(timeoutId);
                    
                    if (!response.ok) {
                        throw new Error(`HTTP ${response.status}`);
                    }
                    
                    return await response.json();
                    
                } catch (err) {
                    // On last attempt, throw error
                    if (attempt === maxRetries - 1) {
                        throw err;
                    }
                    
                    // Exponential backoff: wait longer between retries
                    await new Promise(resolve => setTimeout(resolve, 1000 * (attempt + 1)));
                }
            }
        };

        /**
         * Debounce function to limit execution rate
         * @param {Function} func - Function to debounce
         * @param {number} delay - Delay in milliseconds
         * @returns {Function} Debounced function
         */
        const debounce = (func, delay) => {
            let timeoutId;
            return (...args) => {
                clearTimeout(timeoutId);
                timeoutId = setTimeout(() => func(...args), delay);
            };
        };

        /**
         * Show visual feedback on element
         * @param {HTMLElement} element - Target element
         * @param {string} type - 'success' or 'error'
         * @param {number} duration - Duration in milliseconds
         */
        const showFeedback = (element, type = 'success', duration = CONFIG.FEEDBACK_DURATION_MS) => {
            const colors = {
                success: { bg: 'rgba(40, 167, 69, 0.2)', color: '#28a745' },
                error: { bg: 'rgba(220, 53, 69, 0.2)', color: '#dc3545' }
            };
            
            const color = colors[type] || colors.success;
            element.style.background = color.bg;
            element.style.color = color.color;
            
            setTimeout(() => {
                element.style.background = '';
                element.style.color = '';
            }, duration);
        };

        /**
         * Safely validate and sanitize URL
         * @param {string} urlString - URL to validate
         * @returns {string|null} Valid URL or null
         */
        const sanitizeUrl = (urlString) => {
            try {
                const url = new URL(urlString);
                // Only allow http and https protocols
                if (['http:', 'https:'].includes(url.protocol)) {
                    return url.href;
                }
            } catch (e) {
                console.warn('Invalid URL:', urlString);
            }
            return null;
        };

        // =====================================================
        // UI Element References
        // =====================================================
        const UI = {
            slider: document.getElementById('brightness'),
            label: document.getElementById('brightness-label'),
            statusBadge: document.getElementById('status'),
            lastUpdate: document.getElementById('last-update'),
            temp: document.getElementById('temp'),
            hum: document.getElementById('hum'),
            desc: document.getElementById('desc'),
            ip: document.getElementById('ip'),
            newsBox: document.getElementById('news')
        };

        /**
         * Updates the "last update" timestamp in the footer
         */
        const updateTimestamp = () => {
            const now = new Date();
            const timeString = now.toLocaleTimeString('it-IT');
            UI.lastUpdate.textContent = timeString;
            UI.lastUpdate.setAttribute('datetime', now.toISOString());
        };

        /**
         * Update status badge
         * @param {boolean} online - Device online status
         */
        const setStatusBadge = (online) => {
            if (online) {
                UI.statusBadge.textContent = '● Online';
                UI.statusBadge.style.background = 'rgba(40, 167, 69, 0.2)';
                UI.statusBadge.style.color = '#28a745';
            } else {
                UI.statusBadge.textContent = '● Offline';
                UI.statusBadge.style.background = 'rgba(220, 53, 69, 0.2)';
                UI.statusBadge.style.color = '#dc3545';
            }
        };

        // =====================================================
        // Data Loading Functions
        // =====================================================
        
        /**
         * Load device status and weather data
         */
        const loadDeviceData = async () => {
            try {
                const data = await fetchWithRetry('/api');
                
                // Update weather display (safely escape data)
                UI.temp.textContent = `${data.temp}°C`;
                UI.hum.textContent = `${data.humidity}%`;
                UI.desc.textContent = data.description || '--';
                UI.ip.textContent = data.ip || '--';

                // Synchronize brightness slider with device's current setting
                UI.slider.value = data.brightness || 0;
                const percentage = Math.round((data.brightness / 255) * 100);
                UI.label.textContent = `${percentage}%`;
                
                // Update ARIA attributes
                UI.slider.setAttribute('aria-valuenow', data.brightness || 0);
                UI.slider.setAttribute('aria-valuetext', `${percentage} percento`);

                // Update connection status badge based on device's reported state (not just fetch success)
                setStatusBadge(data.online === true);
                updateTimestamp();
                
            } catch (err) {
                console.error('Error fetching device data:', err);
                // Only mark offline on fetch failure - device may have lost connectivity
                setStatusBadge(false);
            }
        };

        /**
         * Load news feed with safe DOM manipulation
         */
        const loadNewsData = async () => {
            try {
                const data = await fetchWithRetry('/news');
                
                // Clear loading message
                UI.newsBox.innerHTML = '';
                
                if (!data.items || data.items.length === 0) {
                    const p = document.createElement('p');
                    p.className = 'text-muted mb-0';
                    p.textContent = 'Nessuna notizia disponibile';
                    UI.newsBox.appendChild(p);
                    return;
                }
                
                // Build news list using DocumentFragment for better performance
                const fragment = document.createDocumentFragment();
                const list = document.createElement('ul');
                list.className = 'ps-3 mb-0';
                
                data.items.forEach(item => {
                    // Validate URL before creating link
                    const sanitizedUrl = sanitizeUrl(item.link);
                    if (!sanitizedUrl && !item.title) return; // Skip invalid items
                    
                    const li = document.createElement('li');
                    li.className = 'mb-2';
                    
                    if (sanitizedUrl) {
                        const a = document.createElement('a');
                        a.href = sanitizedUrl;
                        a.textContent = item.title || '(senza titolo)';
                        a.setAttribute('target', '_blank');
                        a.setAttribute('rel', 'noopener noreferrer');
                        li.appendChild(a);
                    } else {
                        // If no valid URL, just show text
                        li.textContent = item.title || '(senza titolo)';
                    }
                    
                    list.appendChild(li);
                });
                
                fragment.appendChild(list);
                UI.newsBox.appendChild(fragment);
                
            } catch (err) {
                console.error('Error fetching news:', err);
                const p = document.createElement('p');
                p.className = 'text-danger mb-0';
                p.textContent = 'Errore nel caricamento delle notizie';
                UI.newsBox.innerHTML = '';
                UI.newsBox.appendChild(p);
            }
        };

        // =====================================================
        // Brightness Control
        // =====================================================
        
        // Update label in real-time while user drags slider
        UI.slider.addEventListener('input', () => {
            const percentage = Math.round((UI.slider.value / 255) * 100);
            UI.label.textContent = `${percentage}%`;
            UI.label.classList.add('opacity-75'); // Visual feedback: pending
            
            // Update ARIA attributes for accessibility
            UI.slider.setAttribute('aria-valuenow', UI.slider.value);
            UI.slider.setAttribute('aria-valuetext', `${percentage} percento`);
        });

        // Send brightness value to device (debounced to reduce requests)
        UI.slider.addEventListener('change', debounce(async () => {
            const value = parseInt(UI.slider.value, 10);
            
            // Validate range
            if (value < 0 || value > 255) {
                showFeedback(UI.label, 'error');
                return;
            }
            
            try {
                await fetchWithRetry(`/brightness?value=${value}`);
                UI.label.classList.remove('opacity-75');
                showFeedback(UI.label, 'success', 1000);
            } catch (err) {
                console.error('Error setting brightness:', err);
                showFeedback(UI.label, 'error');
            }
        }, CONFIG.BRIGHTNESS_DEBOUNCE_MS));

        // =====================================================
        // Application Initialization
        // =====================================================
        
        // Load initial data
        (async () => {
            await Promise.all([
                loadDeviceData(),
                loadNewsData()
            ]);
        })();

        // Auto-refresh device data and news every 60 seconds
        setInterval(() => {
            loadDeviceData();
            loadNewsData();
        }, CONFIG.AUTO_REFRESH_INTERVAL_MS);
    </script>
</body>

</html>
)";
