/* ── ClaVel Reactive — c-get, c-post, c-target ───────────────────────
 * Intercepta atributos c-get / c-post en cualquier elemento clickeable.
 * Envía la petición con la cabecera X-ClaVel-Request: true para que
 * el servidor devuelva solo el fragmento (View_render_partial).
 * Reemplaza el contenido del elemento indicado en c-target usando
 * document.startViewTransition() cuando está disponible.
 */
(function () {
  function csrfToken() {
    const meta = document.querySelector('meta[name="csrf-token"]');
    if (meta) return meta.getAttribute('content');
    const input = document.querySelector('input[name="csrf_token"]');
    return input ? input.value : '';
  }

  async function clavRequest(url, method, extraBody) {
    const headers = { 'X-ClaVel-Request': 'true' };
    let body;
    if (method === 'POST') {
      const params = new URLSearchParams(extraBody || {});
      const token = csrfToken();
      if (token) params.set('csrf_token', token);
      body = params.toString();
      headers['Content-Type'] = 'application/x-www-form-urlencoded';
    }
    const res = await fetch(url, { method, headers, body });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    return res.text();
  }

  function applyHtml(target, html) {
    if ('startViewTransition' in document) {
      document.startViewTransition(() => { target.innerHTML = html; });
    } else {
      target.innerHTML = html;
    }
  }

  document.addEventListener('click', async (e) => {
    const el = e.target.closest('[c-get],[c-post]');
    if (!el) return;
    e.preventDefault();

    const url    = el.getAttribute('c-get') || el.getAttribute('c-post');
    const method = el.hasAttribute('c-get') ? 'GET' : 'POST';
    const sel    = el.getAttribute('c-target');
    const target = sel ? document.querySelector(sel) : el;
    if (!target) return;

    el.setAttribute('aria-busy', 'true');
    try {
      const html = await clavRequest(url, method);
      applyHtml(target, html);
    } catch (err) {
      console.error('[ClaVel]', err);
    } finally {
      el.removeAttribute('aria-busy');
    }
  });

  /* Soporte para c-submit en formularios */
  document.addEventListener('submit', async (e) => {
    const form = e.target.closest('form[c-submit]');
    if (!form) return;
    e.preventDefault();

    const url    = form.getAttribute('action') || window.location.pathname;
    const method = (form.getAttribute('method') || 'POST').toUpperCase();
    const sel    = form.getAttribute('c-target');
    const target = sel ? document.querySelector(sel) : form;
    if (!target) return;

    const data = Object.fromEntries(new FormData(form));
    form.setAttribute('aria-busy', 'true');
    try {
      const html = await clavRequest(url, method, data);
      applyHtml(target, html);
    } catch (err) {
      console.error('[ClaVel]', err);
    } finally {
      form.removeAttribute('aria-busy');
    }
  });
})();

/* ── Code tabs ──────────────────────────────────────────────────────── */
document.addEventListener('DOMContentLoaded', () => {
  /* Tab switching in code showcase */
  document.querySelectorAll('.code-tab').forEach(tab => {
    tab.addEventListener('click', () => {
      const group = tab.closest('.code-showcase');
      group.querySelectorAll('.code-tab').forEach(t => t.classList.remove('active'));
      group.querySelectorAll('.code-panel').forEach(p => p.classList.remove('active'));
      tab.classList.add('active');
      const target = group.querySelector(`.code-panel[data-tab="${tab.dataset.tab}"]`);
      if (target) target.classList.add('active');
    });
  });

  /* Intersection Observer — animate cards on scroll */
  const observer = new IntersectionObserver((entries) => {
    entries.forEach(entry => {
      if (entry.isIntersecting) {
        entry.target.style.animationPlayState = 'running';
        observer.unobserve(entry.target);
      }
    });
  }, { threshold: 0.1 });

  document.querySelectorAll('.feature-card, .timeline-item').forEach(el => {
    el.style.animationPlayState = 'paused';
    observer.observe(el);
  });

  /* Active nav link */
  const path = window.location.pathname;
  document.querySelectorAll('.nav-links a').forEach(a => {
    if (a.getAttribute('href') === path) a.classList.add('active');
  });
});
