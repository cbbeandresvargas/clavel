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
