/**
 * ClaVel.js - Native View Transitions & Partial Rendering
 * Un micro-script (< 2KB) que intercepta interacciones y renderiza partes de la UI
 * usando la API moderna de ViewTransitions.
 */

/* ── Enlaces y Botones ────────────────────────────────────────────── */
document.addEventListener('click', async (e) => {
    let el = e.target.closest('[c-get], [c-post], a[c-target]');
    if (!el) return;
    
    e.preventDefault();
    
    let url = el.getAttribute('c-get') || el.getAttribute('c-post') || el.getAttribute('href');
    let method = el.hasAttribute('c-post') ? 'POST' : 'GET';
    let targetSelector = el.getAttribute('c-target') || 'main';
    
    let targetEl = document.querySelector(targetSelector);
    if (!targetEl) {
        console.warn('[clavel.js] Target no encontrado:', targetSelector);
        return;
    }

    try {
        let options = {
            method,
            headers: { 'X-ClaVel-Request': 'true' }
        };
        
        /* Enviar token CSRF si existe (para peticiones POST seguras) */
        if (method === 'POST') {
            let csrf = document.querySelector('input[name="csrf_token"]');
            if (csrf) options.headers['X-CSRF-TOKEN'] = csrf.value;
        }
        
        let res = await fetch(url, options);
        if (res.status >= 400) {
            console.error('[clavel.js] Error HTTP', res.status);
            return;
        }
        
        let html = await res.text();
        
        /* Usa la API moderna ViewTransitions para suavidad, con fallback clásico */
        if (document.startViewTransition) {
            document.startViewTransition(() => { targetEl.innerHTML = html; });
        } else {
            targetEl.innerHTML = html;
        }
        
    } catch (err) {
        console.error('[clavel.js] Error de conexión:', err);
    }
});

/* ── Formularios ──────────────────────────────────────────────────── */
document.addEventListener('submit', async (e) => {
    let form = e.target;
    if (!form.hasAttribute('c-post') && !form.hasAttribute('c-get') && !form.hasAttribute('c-target')) return;
    
    e.preventDefault();
    
    let url = form.getAttribute('action') || form.getAttribute('c-post') || form.getAttribute('c-get') || window.location.href;
    let method = form.getAttribute('method') || (form.hasAttribute('c-get') ? 'GET' : 'POST');
    let targetSelector = form.getAttribute('c-target') || 'main';
    
    let targetEl = document.querySelector(targetSelector);
    if (!targetEl) return;
    
    try {
        let formData = new FormData(form);
        let options = {
            method: method.toUpperCase(),
            headers: { 'X-ClaVel-Request': 'true' },
        };
        
        if (options.method === 'GET') {
            let params = new URLSearchParams(formData).toString();
            url += (url.includes('?') ? '&' : '?') + params;
        } else {
            /* En un fetch con FormData no ponemos Content-Type; el navegador lo hace con boundary */
            options.body = formData;
        }
        
        let res = await fetch(url, options);
        if (res.status >= 400) return;
        
        let html = await res.text();
        
        if (document.startViewTransition) {
            document.startViewTransition(() => { targetEl.innerHTML = html; });
        } else {
            targetEl.innerHTML = html;
        }
    } catch (err) {
        console.error('[clavel.js] Form Submit Error:', err);
    }
});
