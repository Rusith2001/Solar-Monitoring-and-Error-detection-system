document.addEventListener('DOMContentLoaded', () => {
    // Firebase Configuration
    const firebaseConfig = {
        apiKey: "AIzaSyAtYmb4k5z2yngjHAID7dtp_0EreCl7DmA",
        authDomain: "fault-monitoring-system.firebaseapp.com",
        databaseURL: "https://fault-monitoring-system-default-rtdb.asia-southeast1.firebasedatabase.app/",
        projectId: "fault-monitoring-system",
        storageBucket: "fault-monitoring-system.appspot.com",
        messagingSenderId: "993126909988"
    };

    // Initialize Firebase with error handling
    let database;
    try {
        firebase.initializeApp(firebaseConfig);
        database = firebase.database();
        console.log('Firebase initialized successfully');
    } catch (error) {
        console.error('Firebase initialization failed:', error);
        showToast('Failed to connect to database. Some features may be unavailable.', 'system');
    }

    // Elements
    const solarVoltage = document.getElementById('solarVoltage');
    const solarCurrent = document.getElementById('solarCurrent');
    const solarTemp = document.getElementById('solarTemp');
    const solarToggle = document.getElementById('solarToggle');

    const controllerVoltage = document.getElementById('controllerVoltage');
    const controllerCurrent = document.getElementById('controllerCurrent');
    const controllerTemp = document.getElementById('controllerTemp');
    const controllerToggle = document.getElementById('controllerToggle');

    const batteryVoltage = document.getElementById('batteryVoltage');
    const batteryCurrent = document.getElementById('batteryCurrent');
    const batteryTemp = document.getElementById('batteryTemp');
    const batteryToggle = document.getElementById('batteryToggle');

    const inverterVoltage = document.getElementById('inverterVoltage');
    const inverterCurrent = document.getElementById('inverterCurrent');
    const inverterTemp = document.getElementById('inverterTemp');
    const inverterToggle = document.getElementById('inverterToggle');

    const userRoleElement = document.getElementById('userRole');
    const logoutBtn = document.getElementById('logoutBtn');
    const alertsContainer = document.getElementById('alerts');

    // Thresholds for modules
    const thresholds = {
        dc_module_1: {
            voltage: { min: 10, max: 15, name: 'Solar Voltage' },
            current: { min: 0.001, max: 0.002, name: 'Solar Current' },
            temperature: { min: 0, max: 50, name: 'Solar Temperature' }
        },
        dc_module_2: {
            voltage: { min: 10, max: 15, name: 'Controller Voltage' },
            current: { min: 0.001, max: 0.012, name: 'Controller Current' },
            temperature: { min: 0, max: 50, name: 'Controller Temperature' }
        },
        dc_module_3: {
            voltage: { min: 10, max: 15, name: 'Battery Voltage' },
            current: { min: 0.1, max: 1, name: 'Battery Current' },
            temperature: { min: 0, max: 50, name: 'Battery Temperature' }
        },
        ac1: {
            voltage: { min: 220, max: 240, name: 'Inverter Voltage' },
            current: { min: 0.01, max: 0.5, name: 'Inverter Current' },
            temperature: { min: 0, max: 60, name: 'Inverter Temperature' }
        }
    };

    // Debounce alerts (30s cooldown per variable)
    const alertCooldowns = {};
    const ALERT_COOLDOWN_MS = 30000; // 30 seconds

    // Cooldowns for power generation notifications
    const powerGenCooldowns = {
        daily: 0,
        monthly: 0
    };

    // Load stored notifications
    const loadStoredNotifications = () => {
        const storedNotifications = JSON.parse(localStorage.getItem('notifications') || '{}');
        for (const id in storedNotifications) {
            showToast(storedNotifications[id].message, id, false); // false to skip storing
        }
    };

    // Function to show or update toast notification
    const showToast = (message, id, save = true) => {
        if (!alertsContainer) {
            console.warn('Alerts container not found');
            return;
        }
        // Check for existing notification with the same id
        let toast = document.querySelector(`[data-id="${id}"]`);
        if (toast) {
            // Update existing notification
            const span = toast.querySelector('span');
            if (span) span.textContent = message;
            return;
        }
        // Create new notification
        toast = document.createElement('div');
        toast.className = 'toast flex justify-between items-center';
        toast.setAttribute('data-id', id);
        toast.innerHTML = `
            <span>${message}</span>
            <button class="text-white ml-4 focus:outline-none">✖</button>
        `;
        alertsContainer.appendChild(toast);
        setTimeout(() => toast.classList.add('show'), 300);
        toast.querySelector('button').addEventListener('click', () => {
            toast.classList.remove('show');
            setTimeout(() => {
                toast.remove();
                // Remove from localStorage
                const storedNotifications = JSON.parse(localStorage.getItem('notifications') || '{}');
                delete storedNotifications[id];
                localStorage.setItem('notifications', JSON.stringify(storedNotifications));
            }, 300);
        });
        // Save to localStorage if save is true
        if (save) {
            const storedNotifications = JSON.parse(localStorage.getItem('notifications') || '{}');
            storedNotifications[id] = { message, id };
            localStorage.setItem('notifications', JSON.stringify(storedNotifications));
        }
    };

    const checkThresholds = (module, data, moduleName) => {
        const timestamp = Date.now();
        const moduleThresholds = thresholds[moduleName];

        ['voltage', 'current', 'temperature'].forEach(param => {
            const value = data[param] || 0;
            const { min, max, name } = moduleThresholds[param];
            const key = `${moduleName}_${param}`;
            
            // Skip if in cooldown
            if (alertCooldowns[key] && timestamp < alertCooldowns[key]) {
                return;
            }

            let message = '';
            if (value < min) {
                message = `${name} too low: ${value.toFixed(2)}`;
                alertCooldowns[key] = timestamp + ALERT_COOLDOWN_MS;
            } else if (value > max) {
                message = `${name} too high: ${value.toFixed(2)}`;
                alertCooldowns[key] = timestamp + ALERT_COOLDOWN_MS;
            }

            if (message) {
                // Append module-specific fault message
                if (moduleName === 'dc_module_1') {
                    message += ' There is a fault or system failure in the solar panel and the solar charge controller. Please check the system between the two.';
                } else if (moduleName === 'dc_module_2') {
                    message += ' There is a fault or system failure in the solar charge controller and the battery. Please check the system between the two.';
                } else if (moduleName === 'dc_module_3') {
                    message += ' There is a fault or system failure in the battery and Inverter. Please check the system between the two.';
                } else if (moduleName === 'ac1') {
                    message += ' There is a fault or system failure in Inverter and load. Please check the system between the two.';
                }
                showToast(message, key);
            }
        });
    };

    // Charts
    let dailyChart, monthlyChart;
    try {
        const dailyChartCtx = document.getElementById('dailyChart')?.getContext('2d');
        if (dailyChartCtx) {
            dailyChart = new Chart(dailyChartCtx, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Power (W)',
                        data: [],
                        borderColor: '#00f7ff',
                        fill: false
                    }]
                },
                options: {
                    responsive: true,
                    scales: {
                        x: { title: { display: true, text: 'Time', color: '#e0e0e0' }, ticks: { color: '#e0e0e0' }, grid: { color: 'rgba(255,255,255,0.1)' } },
                        y: { title: { display: true, text: 'Power (W)', color: '#e0e0e0' }, ticks: { color: '#e0e0e0' }, grid: { color: 'rgba(255,255,255,0.1)' } }
                    },
                    plugins: {
                        legend: { labels: { color: '#e0e0e0' } }
                    }
                }
            });
        }

        const monthlyChartCtx = document.getElementById('monthlyChart')?.getContext('2d');
        if (monthlyChartCtx) {
            monthlyChart = new Chart(monthlyChartCtx, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Power (kWh)',
                        data: [],
                        borderColor: '#ff00ff',
                        fill: false
                    }]
                },
                options: {
                    responsive: true,
                    scales: {
                        x: { title: { display: true, text: 'Day', color: '#e0e0e0' }, ticks: { color: '#e0e0e0' }, grid: { color: 'rgba(255,255,237,0.1)' } },
                        y: { title: { display: true, text: 'Power (kWh)', color: '#e0e0e0' }, ticks: { color: '#e0e0e0' }, grid: { color: 'rgba(255,255,255,0.1)' } }
                    },
                    plugins: {
                        legend: { labels: { color: '#e0e0e0' } }
                    }
                }
            });
        }
        console.log('Charts initialized successfully');
    } catch (error) {
        console.error('Chart initialization failed:', error);
        showToast('Failed to initialize charts.', 'system');
    }

    // Check if user is logged in
    const currentUser = JSON.parse(localStorage.getItem('currentUser') || '{}');
    if (window.location.pathname.includes('dashboard.html')) {
        if (!localStorage.getItem('currentUser')) {
            console.log('No valid user, redirecting to index.html');
            window.location.href = 'index.html';
            return;
        } else {
            console.log('Current user:', currentUser);
            const role = currentUser.role ? currentUser.role.charAt(0).toUpperCase() + currentUser.role.slice(1) : 'User';
            if (userRoleElement) userRoleElement.textContent = role;
            if (currentUser.role && currentUser.role.toLowerCase() === 'admin') {
                console.log('Enabling admin toggles');
                if (solarToggle) solarToggle.disabled = false;
                if (controllerToggle) controllerToggle.disabled = false;
                if (batteryToggle) batteryToggle.disabled = false;
                if (inverterToggle) inverterToggle.disabled = false;
            }
            // Load stored notifications
            loadStoredNotifications();
        }
    }

    // Google Sheets Configuration
    const APPS_URL = 'https://script.google.com/macros/s/AKfycbyX6kb_Mh3J90zXT75eeauNCCbDPFGTPdxj1wqF9cHRlKOxHv8FFBf61srdlQeudjE/exec';
    const SHEET_NAME = 'dc_module_1';

    // Update daily chart
    async function updateDailyChart() {
        try {
            const today = new Date().toISOString().split('T')[0];
            const response = await fetch(APPS_URL);
            if (!response.ok) {
                const errorText = await response.text();
                throw new Error(`HTTP error! status: ${response.status}, message: ${errorText}`);
            }
            const data = await response.json();
            console.log('Raw data from server (daily):', data);

            const rows = data.values || [];
            if (rows.length === 0) {
                console.warn('No data found in Google Sheet');
                showToast('No data available for daily graph.', 'daily_error');
                return;
            }

            // Filter data for today
            const labels = [];
            const powerValues = [];
            let totalDailyPower = 0;
            rows.slice(1).forEach((row, index) => { // Skip header row
                const dateTime = row[0]; // Column A: Date and Time
                const power = parseFloat(row[3]); // Column D: Power
                if (!dateTime || isNaN(power)) {
                    console.warn(`Invalid row ${index + 2}:`, { dateTime, power: row[3] });
                    return;
                }
                try {
                    const date = new Date(dateTime).toISOString().split('T')[0];
                    if (date === today) {
                        const time = new Date(dateTime).toLocaleTimeString();
                        labels.push(time);
                        powerValues.push(power);
                        totalDailyPower += power;
                    }
                } catch (e) {
                    console.warn(`Invalid timestamp in row ${index + 2}:`, dateTime);
                }
            });

            if (labels.length === 0) {
                console.warn('No data found for today:', today);
                showToast('No data available for today’s graph.', 'daily_error');
            } else {
                console.log('Daily chart data:', { labels, powerValues });
            }

            // Check daily power generation
            const timestamp = Date.now();
            if (timestamp > powerGenCooldowns.daily) {
                if (totalDailyPower > 0.5) {
                    showToast('Your Solar system works well.', 'daily_power');
                } else if (totalDailyPower < 0.5) {
                    showToast('Your Solar system\'s generation is too low. Please check the system.', 'daily_power');
                }
                powerGenCooldowns.daily = timestamp + ALERT_COOLDOWN_MS;
            }

            if (dailyChart) {
                dailyChart.data.labels = labels;
                dailyChart.data.datasets[0].data = powerValues;
                dailyChart.update();
            }
        } catch (error) {
            console.error('Error updating daily chart:', error);
            showToast('Error fetching daily graph data: ' + error.message, 'daily_error');
        }
    }

    // Update monthly chart
    async function updateMonthlyChart() {
        try {
            const currentDate = new Date();
            const year = currentDate.getFullYear();
            const month = (currentDate.getMonth() + 1).toString().padStart(2, '0');
            const yearMonth = `${year}-${month}`;
            const daysInMonth = new Date(year, currentDate.getMonth() + 1, 0).getDate();
            const labels = Array.from({ length: daysInMonth }, (_, i) => `${i + 1}`);
            const powerValues = Array(daysInMonth).fill(0);

            const response = await fetch(APPS_URL);
            if (!response.ok) {
                const errorText = await response.text();
                throw new Error(`HTTP error! status: ${response.status}, message: ${errorText}`);
            }
            const data = await response.json();
            console.log('Raw data from server (monthly):', data);

            const rows = data.values || [];
            if (rows.length === 0) {
                console.warn('No data found in Google Sheet');
                showToast('No data available for monthly graph.', 'monthly_error');
                return;
            }

            // Calculate daily power sums (kWh)
            const dailyPowerSums = {};
            let totalMonthlyPower = 0;
            rows.slice(1).forEach((row, index) => {
                const dateTime = row[0];
                const powerW = parseFloat(row[3]);
                if (!dateTime || isNaN(powerW)) {
                    console.warn(`Invalid row ${index + 2}:`, { dateTime, power: row[3] });
                    return;
                }
                try {
                    const dateObj = new Date(dateTime);
                    if (!dateObj.getTime()) {
                        console.warn(`Invalid timestamp in row ${index + 2}:`, dateTime);
                        return;
                    }
                    const date = dateObj.toISOString().split('T')[0];
                    if (date.startsWith(yearMonth)) {
                        const day = parseInt(date.split('-')[2], 10) - 1;
                        dailyPowerSums[date] = (dailyPowerSums[date] || 0) + (powerW * (5 / 3600)); // kWh
                        totalMonthlyPower += powerW * (5 / 3600);
                    }
                } catch (e) {
                    console.warn(`Invalid timestamp in row ${index + 2}:`, dateTime);
                }
            });

            Object.keys(dailyPowerSums).forEach(date => {
                const day = parseInt(date.split('-')[2], 10) - 1;
                if (day >= 0 && day < daysInMonth) {
                    powerValues[day] = parseFloat(dailyPowerSums[date].toFixed(2));
                }
            });

            console.log('Monthly chart data:', { labels, powerValues });

            // Check monthly power generation
            const timestamp = Date.now();
            if (timestamp > powerGenCooldowns.monthly) {
                if (totalMonthlyPower > 0.5) {
                    showToast('Your Solar system works well.', 'monthly_power');
                } else if (totalMonthlyPower < 0.5) {
                    showToast('Your Solar system\'s generation is too low. Please check the system.', 'monthly_power');
                }
                powerGenCooldowns.monthly = timestamp + ALERT_COOLDOWN_MS;
            }

            if (monthlyChart) {
                monthlyChart.data.labels = labels;
                monthlyChart.data.datasets[0].data = powerValues;
                monthlyChart.update();
            }
        } catch (error) {
            console.error('Error updating monthly chart:', error);
            showToast('Error fetching monthly graph data: ' + error.message, 'monthly_error');
        }
    }

    // Initialize charts
    if (window.location.pathname.includes('dashboard.html')) {
        updateDailyChart();
        updateMonthlyChart();

        // Periodic chart updates
        setInterval(updateDailyChart, 60000);
        setInterval(() => {
            const currentDate = new Date();
            if (currentDate.getHours() === 0 && currentDate.getMinutes() === 0) {
                updateMonthlyChart();
            }
        }, 60000);
    }

    // Real-time sensor data updates
    if (database && window.location.pathname.includes('dashboard.html')) {
        try {
            database.ref('modules/dc_module_1').on('value', (snapshot) => {
                const data = snapshot.val() || {};
                solarVoltage.textContent = (data.voltage || 0).toFixed(2);
                solarCurrent.textContent = (data.current || 0).toFixed(2);
                solarTemp.textContent = (data.temperature || 0).toFixed(2);
                solarToggle.checked = data.relayState || false;
                checkThresholds('dc_module_1', data, 'dc_module_1');
                console.log('dc_module_1 data:', data);
            });

            database.ref('modules/dc_module_2').on('value', (snapshot) => {
                const data = snapshot.val() || {};
                controllerVoltage.textContent = (data.voltage || 0).toFixed(2);
                controllerCurrent.textContent = (data.current || 0).toFixed(2);
                controllerTemp.textContent = (data.temperature || 0).toFixed(2);
                controllerToggle.checked = data.relayState || false;
                checkThresholds('dc_module_2', data, 'dc_module_2');
                console.log('dc_module_2 data:', data);
            });

            database.ref('modules/dc_module_3').on('value', (snapshot) => {
                const data = snapshot.val() || {};
                batteryVoltage.textContent = (data.voltage || 0).toFixed(2);
                batteryCurrent.textContent = (data.current || 0).toFixed(2);
                batteryTemp.textContent = (data.temperature || 0).toFixed(2);
                batteryToggle.checked = data.relayState || false;
                checkThresholds('dc_module_3', data, 'dc_module_3');
                console.log('dc_module_3 data:', data);
            });

            database.ref('modules/ac1').on('value', (snapshot) => {
                const data = snapshot.val() || {};
                inverterVoltage.textContent = (data.voltage || 0).toFixed(2);
                inverterCurrent.textContent = ((data.current || 0) / 1000).toFixed(2); // Convert mA to A
                inverterTemp.textContent = (data.temperature || 0).toFixed(2);
                inverterToggle.checked = data.relayState || false;
                checkThresholds('ac1', { ...data, current: (data.current || 0) / 1000 }, 'ac1');
                console.log('ac1 data:', data);
            });
        } catch (error) {
            console.error('Firebase listener error:', error);
            showToast('Error fetching sensor data.', 'system');
        }
    }

    // Toggle switches for admins
    if (currentUser.role && currentUser.role.toLowerCase() === 'admin' && database && window.location.pathname.includes('dashboard.html')) {
        const toggles = [
            { element: solarToggle, path: 'dc_module_1', name: 'Solar System' },
            { element: controllerToggle, path: 'dc_module_2', name: 'Charge Controller' },
            { element: batteryToggle, path: 'dc_module_3', name: 'Battery' },
            { element: inverterToggle, path: 'ac1', name: 'Inverter' }
        ];

        toggles.forEach(toggle => {
            if (toggle.element) {
                toggle.element.addEventListener('change', () => {
                    const confirmAction = confirm(`Are you sure you want to turn ${toggle.name} ${toggle.element.checked ? 'On' : 'Off'}?`);
                    if (confirmAction) {
                        try {
                            database.ref(`modules/${toggle.path}/relayState`).set(toggle.element.checked);
                            showToast(`${toggle.name} Power Turned ${toggle.element.checked ? 'On' : 'Off'}`, `${toggle.path}_toggle`);
                            console.log(`${toggle.name} toggle set to ${toggle.element.checked}`);
                        } catch (error) {
                            console.error(`Error updating ${toggle.name} relayState:`, error);
                            showToast(`Error updating ${toggle.name} power state.`, 'system');
                            toggle.element.checked = !toggle.element.checked; // Revert toggle on error
                        }
                    } else {
                        toggle.element.checked = !toggle.element.checked; // Revert toggle if canceled
                        console.log(`${toggle.name} toggle canceled`);
                    }
                });
            } else {
                console.error(`${toggle.name} toggle element not found`);
            }
        });
    } else if (!database && window.location.pathname.includes('dashboard.html')) {
        console.error('No Firebase database connection for toggles');
        showToast('Cannot control relays: No database connection.', 'system');
    }

    // Download buttons
    if (window.location.pathname.includes('dashboard.html')) {
        try {
            document.getElementById('downloadDailyGraph')?.addEventListener('click', () => {
                const link = document.createElement('a');
                link.href = dailyChart.toBase64Image();
                link.download = 'daily_power_generation.png';
                link.click();
            });

            document.getElementById('downloadDailyData')?.addEventListener('click', () => {
                const data = dailyChart.data.datasets[0].data;
                const labels = dailyChart.data.labels;
                const csv = labels.map((label, i) => `${label},${data[i]}`).join('\n');
                const blob = new Blob([`Time,Power (W)\n${csv}`], { type: 'text/csv' });
                const link = document.createElement('a');
                link.href = URL.createObjectURL(blob);
                link.download = 'daily_power_data.csv';
                link.click();
            });

            document.getElementById('downloadMonthlyGraph')?.addEventListener('click', () => {
                const link = document.createElement('a');
                link.href = monthlyChart.toBase64Image();
                link.download = 'monthly_power_generation.png';
                link.click();
            });

            document.getElementById('downloadMonthlyData')?.addEventListener('click', () => {
                const data = monthlyChart.data.datasets[0].data;
                const labels = monthlyChart.data.labels;
                const csv = labels.map((label, i) => `${label},${data[i]}`).join('\n');
                const blob = new Blob([`Day,Power (kWh)\n${csv}`], { type: 'text/csv' });
                const link = document.createElement('a');
                link.href = URL.createObjectURL(blob);
                link.download = 'monthly_power_data.csv';
                link.click();
            });
        } catch (error) {
            console.error('Download button error:', error);
            showToast('Error setting up download buttons.', 'system');
        }
    }
});