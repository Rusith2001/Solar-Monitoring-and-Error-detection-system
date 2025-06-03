// Elements
const loginTab = document.getElementById('loginTab');
const signupTab = document.getElementById('signupTab');
const loginForm = document.getElementById('loginForm');
const signupForm = document.getElementById('signupForm');

const loginEmail = document.getElementById('loginEmail');
const loginPassword = document.getElementById('loginPassword');
const loginBtn = document.getElementById('loginBtn');
const loginError = document.getElementById('loginError');

const signupEmail = document.getElementById('signupEmail');
const signupPassword = document.getElementById('signupPassword');
const signupRole = document.getElementById('signupRole');
const signupBtn = document.getElementById('signupBtn');
const signupError = document.getElementById('signupError');

const adminPinContainer = document.getElementById('adminPinContainer');
const adminPin = document.getElementById('adminPin');

// Check if user is already logged in
const currentUser = JSON.parse(localStorage.getItem('currentUser') || '{}');
if (currentUser && currentUser.email && currentUser.role) {
    console.log('Logged-in user found:', currentUser);
    window.location.href = 'dashboard.html';
}

signupRole.addEventListener('change', () => {
    if (signupRole.value === 'admin') {
        adminPinContainer.classList.remove('hidden');
        adminPin.required = true;
    } else {
        adminPinContainer.classList.add('hidden');
        adminPin.required = false;
    }
    clearError(signupError);
});

// Helper Functions
const showError = (element, message) => {
    element.textContent = message;
    element.classList.remove('opacity-0');
    element.classList.add('opacity-100');
};

const clearError = (element) => {
    element.textContent = '';
    element.classList.add('opacity-0');
    element.classList.remove('opacity-100');
};

const validateEmail = (email) => {
    const re = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
    return re.test(email);
};

const getUsers = () => {
    try {
        return JSON.parse(localStorage.getItem('users')) || [];
    } catch (e) {
        console.error('Error reading users from localStorage:', e);
        return [];
    }
};

const saveUsers = (users) => {
    try {
        localStorage.setItem('users', JSON.stringify(users));
    } catch (e) {
        console.error('Error saving users to localStorage:', e);
        showError(signupError, 'Failed to save user data. Try again.');
    }
};

// Toggle between Login and Signup
loginTab.addEventListener('click', () => {
    loginForm.classList.remove('hidden');
    signupForm.classList.add('hidden');
    loginTab.classList.add('text-white');
    loginTab.classList.remove('text-gray-400');
    signupTab.classList.add('text-gray-400');
    signupTab.classList.remove('text-white');
    clearError(loginError);
    clearError(signupError);
});

signupTab.addEventListener('click', () => {
    signupForm.classList.remove('hidden');
    loginForm.classList.add('hidden');
    signupTab.classList.add('text-white');
    signupTab.classList.remove('text-gray-400');
    loginTab.classList.add('text-gray-400');
    loginTab.classList.remove('text-white');
    clearError(loginError);
    clearError(signupError);
});

// Form Submission Handlers
loginForm.addEventListener('submit', (e) => {
    e.preventDefault();
    const email = loginEmail.value.trim();
    const password = loginPassword.value;

    clearError(loginError);

    if (!email || !password) {
        showError(loginError, 'Please fill in all fields');
        return;
    }

    if (!validateEmail(email)) {
        showError(loginError, 'Please enter a valid email');
        return;
    }

    const users = getUsers();
    const user = users.find(user => user.email === email && user.password === password);

    if (!user) {
        showError(loginError, 'Invalid email or password');
        return;
    }

    console.log('Logging in user:', user);
    localStorage.setItem('currentUser', JSON.stringify(user));
    window.location.href = 'dashboard.html';
});

signupForm.addEventListener('submit', (e) => {
    e.preventDefault();
    const email = signupEmail.value.trim();
    const password = signupPassword.value;
    const role = signupRole.value;
    const pin = adminPin.value;

    clearError(signupError);

    if (!email || !password || !role) {
        showError(signupError, 'Please fill in all fields');
        return;
    }

    if (!validateEmail(email)) {
        showError(signupError, 'Please enter a valid email');
        return;
    }

    if (password.length < 6) {
        showError(signupError, 'Password must be at least 6 characters');
        return;
    }

    if (!['user', 'admin'].includes(role)) {
        showError(signupError, 'Invalid role selected');
        return;
    }

    // Add PIN verification for admin
    if (role === 'admin' && pin !== '1111') {
        showError(signupError, 'Invalid admin PIN');
        return;
    }

    const users = getUsers();
    const userExists = users.some(user => user.email === email);

    if (userExists) {
        showError(signupError, 'Email already exists');
        return;
    }

    const newUser = { email, password, role };
    users.push(newUser);
    saveUsers(users);

    console.log('Signing up user:', newUser);
    localStorage.setItem('currentUser', JSON.stringify(newUser));
    window.location.href = 'dashboard.html';
});