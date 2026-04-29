#pragma once
// =====================================================================
// GTT Web Page HTML (Stored in Flash Memory)
// =====================================================================
// This page mirrors the visual style of the main dashboard and loads
// bus times from /gtt_data.

const char GTT_HTML[] PROGMEM = R"(
<!DOCTYPE html>
<html lang="it">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>MeteoClock</title>
    <script>
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

        .header {
            background: white;
            border-radius: 1rem;
            padding: 1.5rem;
            margin-bottom: 2rem;
            border: 1px solid rgba(0, 0, 0, 0.08);
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.05);
        }

        [data-bs-theme="dark"] .header,
        [data-bs-theme="dark"] .card {
            background: rgba(23, 28, 40, 0.92);
            border: 1px solid rgba(255, 255, 255, 0.12);
            box-shadow: 0 8px 28px rgba(0, 0, 0, 0.28);
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

        .card {
            background: white;
            border: 1px solid rgba(0, 0, 0, 0.08);
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.05);
            transition: transform 0.3s ease, box-shadow 0.3s ease;
        }

        .card:hover {
            transform: translateY(-5px);
            box-shadow: 0 8px 24px rgba(0, 0, 0, 0.1);
        }

        .bus-card {
            position: relative;
            overflow: hidden;
        }

        .bus-card::before {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            right: 0;
            height: 4px;
            background: linear-gradient(90deg, #FFD93D, #FF6B35);
        }

        .line-num {
            color: #FF6B35;
            font-weight: 700;
            font-size: 1.2rem;
            white-space: nowrap;
        }

        .bus-line-container {
            margin-bottom: 1rem;
        }

        .line-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            gap: 0.8rem;
        }

        .bus-times-group {
            display: flex;
            flex-wrap: wrap;
            gap: 0.5rem;
            justify-content: flex-end;
        }

        .time-badge {
            display: inline-block;
            padding: 0.45rem 0.8rem;
            border-radius: 1rem;
            font-weight: 600;
            color: #fff;
            margin: 0;
            font-size: 0.95rem;
            transition: transform 0.2s ease, box-shadow 0.2s ease;
        }

        .time-badge:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
        }

        .time-badge.rt {
            background: linear-gradient(135deg, #28a745, #20c997);
        }

        .time-badge.sch {
            background: linear-gradient(135deg, #6c757d, #868e96);
        }

        .btn-custom {
            font-family: 'Oswald', sans-serif;
            background: linear-gradient(135deg, #FFD93D 0%, #FF6B35 100%);
            color: white;
            border: none;
            font-weight: 600;
            transition: all 0.3s ease;
        }

        .btn-custom:hover {
            color: white;
            transform: translateY(-2px);
            box-shadow: 0 8px 20px rgba(255, 107, 53, 0.4);
        }

        .status {
            opacity: 0.8;
            font-size: 0.95rem;
        }

        [data-bs-theme="dark"] .status {
            color: rgba(232, 235, 240, 0.82);
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

        .status-footer {
            display: flex;
            flex-wrap: wrap;
            justify-content: space-between;
            gap: 1rem;
            align-items: center;
        }

        .status-item {
            flex: 1;
            min-width: 150px;
        }

        p,
        span,
        small,
        a,
        button {
            font-family: 'Oswald', sans-serif;
        }

        @media (max-width: 768px) {
            .container {
                padding-left: 1rem;
                padding-right: 1rem;
            }

            .header {
                padding: 1rem;
            }

            .header h1 {
                font-size: 1.8rem;
            }
        }
    </style>
</head>

<body>
    <div class="container py-4">
        <div class="header">
            <h1>BUS GTT</h1>
            <p class="mb-0 status">Prossimi passaggi - Fermata Sabotino Cap.</p>
        </div>

        <div id="content" class="card p-4 rounded-4">
            <div class="text-center py-5">
                <div class="spinner-border text-warning" role="status" aria-label="Caricamento">
                    <span class="visually-hidden">Caricamento...</span>
                </div>
                <p class="mt-3 mb-0">Caricamento...</p>
            </div>
        </div>

        <div class="card p-3 mt-3 rounded-4">
            <div class="status-footer">
                <div class="status-item"><span class="status-badge" id="last-update">Aggiornamento in corso...</span></div>
            </div>
        </div>

        <div class="mt-3">
            <a href="/" class="btn btn-custom btn-lg w-100 rounded-4">HOME</a>
        </div>
    </div>

    <script>
        function renderStops(data) {
            const content = document.getElementById('content');
            const updateInfo = document.getElementById('last-update');

            if (!data.stops || data.stops.length === 0) {
                content.innerHTML = '<p class="text-center mb-0">Dati non disponibili</p>';
                if (data.debug?.error) {
                    console.error('GTT Error:', data.debug.error);
                }
            } else {
                const grouped = {};
                data.stops.forEach((stop) => {
                    if (!grouped[stop.line]) grouped[stop.line] = [];
                    grouped[stop.line].push(stop);
                });

                let html = '';
                // Keep server order so secondary-stop lines stay at the end.
                Object.keys(grouped).forEach((line) => {
                    html += '<div class="bus-line-container">';
                    html += '<div class="card bus-card p-3 rounded-4">';
                    html += '<div class="line-row">';
                    html += '<div class="line-num">Linea ' + line + '</div>';
                    html += '<div class="bus-times-group">';
                    grouped[line].forEach((stop) => {
                        const cls = stop.realtime ? 'rt' : 'sch';
                        html += '<span class="time-badge ' + cls + '" title="' + (stop.realtime ? 'Tempo reale' : 'Orario previsto') + '">' + stop.hour + '</span>';
                    });
                    html += '</div></div></div></div>';
                });
                content.innerHTML = html;
            }

            const ageSeconds = Math.max(0, Math.floor((data.debug?.age_ms ?? 0) / 1000));
            updateInfo.textContent = 'Aggiornato: ' + ageSeconds + 's fa';
        }

        fetch('/gtt_data')
            .then(async (r) => {
                if (!r.ok) {
                    throw new Error('HTTP ' + r.status);
                }
                return r.json();
            })
            .then((data) => renderStops(data))
            .catch((err) => {
                console.error('GTT Fetch Error:', err);
                document.getElementById('content').innerHTML =
                    '<p class="text-center mb-0">Dati non disponibili</p>';
            });
    </script>
</body>

</html>
)";
