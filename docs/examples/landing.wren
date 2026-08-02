// --- CSS Grid Landing Page Showcase ---
// Demonstrates: inline HTML, CSS Grid, custom properties, backdrop-filter,
// conditional rendering (&&, ternary), iteration (map), semantic HTML.
// Change `var isAnnual = true` below to see annual pricing toggle.

var showBanner = true
var isAnnual = false

var heroTitle = "Build faster with the tools you already know"
var heroSubtitle = "Bialet combines modern CSS architecture, semantic HTML, and server-side rendering into a single cohesive platform — all powered by a single binary."

var features = [
  {"icon": "⚡", "title": "Instant Hot Reload", "desc": "See changes the moment you save. No rebuilds, no waiting, no context switching.", "highlight": true},
  {"icon": "🧩", "title": "Component Architecture", "desc": "Compose pages from reusable Wren classes. Clean separation of logic and presentation, no framework lock-in.", "highlight": false},
  {"icon": "🗄️", "title": "Zero-Config Database", "desc": "SQLite baked into the binary. Write SQL with backtick templates and parameterized queries — no ORM.", "highlight": false},
  {"icon": "🎨", "title": "CSS Grid First", "desc": "Every layout uses CSS Grid. Responsive by default with auto-fit and minmax — no media-query spaghetti.", "highlight": true},
  {"icon": "🔒", "title": "Security Built-In", "desc": "SQL injection prevention, CSRF tokens, XSS escaping, and session management — all out of the box.", "highlight": false},
  {"icon": "📦", "title": "Single Binary Deploy", "desc": "No node_modules. No Docker. No package.json. One binary, one folder — copy it anywhere and run.", "highlight": true},
]

var stats = [
  {"value": "12K+", "label": "Active Developers"},
  {"value": "99.9\x25", "label": "Uptime SLA"},
  {"value": "<2ms", "label": "Avg Response"},
  {"value": "150+", "label": "Plugins"},
]

var pricing = [
  {"name": "Starter", "monthly": "$0", "annual": "$0", "desc": "Perfect for side projects and learning the ropes.", "popular": false, "features": [
    {"text": "Up to 3 projects", "included": true},
    {"text": "500MB storage", "included": true},
    {"text": "Community support", "included": true},
    {"text": "Custom domain", "included": false},
    {"text": "Team access", "included": false},
  ]},
  {"name": "Pro", "monthly": "$29", "annual": "$24", "desc": "For professionals shipping real products.", "popular": true, "features": [
    {"text": "Unlimited projects", "included": true},
    {"text": "50GB storage", "included": true},
    {"text": "Priority support", "included": true},
    {"text": "Custom domain", "included": true},
    {"text": "Team access (5 seats)", "included": true},
  ]},
  {"name": "Enterprise", "monthly": "$99", "annual": "$83", "desc": "For large teams with advanced needs.", "popular": false, "features": [
    {"text": "Everything in Pro", "included": true},
    {"text": "500GB storage", "included": true},
    {"text": "24/7 phone support", "included": true},
    {"text": "SSO integration", "included": true},
    {"text": "Unlimited seats", "included": true},
  ]},
]

var testimonials = [
  {"quote": "Bialet transformed how our team ships. We went from 3-day deploy cycles to pushing 5 times a day. The single-binary approach eliminated our entire infrastructure headache.", "name": "Sarah Chen", "role": "CTO at StreamScale"},
  {"quote": "I evaluated 8 frameworks before choosing Bialet. The combination of Wren's clean syntax with SQLite's simplicity is unbeatable for internal tools and dashboards.", "name": "Marcus Rivera", "role": "Lead Engineer at DataForge"},
  {"quote": "No more fighting with Webpack configs or chasing ESM compatibility. Bialet just works. Our team's velocity doubled within the first sprint after migrating.", "name": "Aiko Tanaka", "role": "Engineering Manager at BuildKit"},
  {"quote": "The CSS Grid-first approach in the documentation was a revelation. Finally, a framework that treats layout as a first-class concern instead of an afterthought.", "name": "James Okonkwo", "role": "Frontend Lead at PixelCraft"},
]

var faqs = [
  {"q": "Do I need to install a database separately?", "a": "No. SQLite is embedded in the Bialet binary. Your database is a single file — back it up by copying it. That's it."},
  {"q": "Can I use my own CSS framework?", "a": "Absolutely. Bialet outputs plain HTML. Link any CSS file — Tailwind, PicoCSS, Bootstrap, or your own custom styles. No conflicts."},
  {"q": "How does deployment work?", "a": "Copy the binary and your app folder to any Linux server. No containers, no runtime dependencies, no environment setup. Works on ARM too."},
  {"q": "Is Wren hard to learn?", "a": "If you know JavaScript or Python, Wren will feel familiar in under an hour. The syntax is minimal and designed for clarity over cleverness."},
  {"q": "What about real-time features?", "a": "Bialet focuses on server-rendered multi-page apps. For WebSocket needs, pair it with a lightweight proxy — the separation keeps both simple."},
  {"q": "Can I run Bialet on a Raspberry Pi?", "a": "Yes. The binary compiles for ARM. People run Nexus on Pi Zeros serving local dashboards, home automation, and small business tools."},
]

var footerLinks = [
  {"title": "Product", "items": [
    {"label": "Features", "href": "#features"},
    {"label": "Pricing", "href": "#pricing"},
    {"label": "Changelog", "href": "#"},
    {"label": "Roadmap", "href": "#"},
  ]},
  {"title": "Resources", "items": [
    {"label": "Documentation", "href": "#"},
    {"label": "Examples", "href": "#"},
    {"label": "API Reference", "href": "#"},
    {"label": "Community", "href": "#"},
  ]},
  {"title": "Company", "items": [
    {"label": "About", "href": "#"},
    {"label": "Blog", "href": "#"},
    {"label": "Twitter", "href": "#"},
    {"label": "GitHub", "href": "#"},
  ]},
]

return <!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Bialet — Modern Framework for People Who Ship</title>
  <style>
/* ═══════════════════════════════════════════════════
   CSS Grid Landing Page — Modern Design System
   Showcases: grid, custom properties, :has(), clamp(),
   backdrop-filter, auto-fit, conditional classes.
   All layout via CSS Grid — no floats, no flex hacks.
   ═══════════════════════════════════════════════════ */

/* --- RESET & BASE --- */
*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

:root {
  --bg: #080810;
  --surface: #111122;
  --surface-raised: #181830;
  --surface-high: #1e1e38;
  --border: #2a2a44;
  --border-light: #333355;
  --text: #e4e4f0;
  --text-secondary: #9090b0;
  --text-muted: #5c5c78;
  --accent: #7c5cfc;
  --accent-light: #a78bfa;
  --accent-glow: rgba(124, 92, 252, 0.2);
  --accent-strong: rgba(124, 92, 252, 0.35);
  --green: #10b981;
  --green-glow: rgba(16, 185, 129, 0.2);
  --amber: #f59e0b;
  --red: #ef4444;
  --radius: 14px;
  --radius-sm: 8px;
  --radius-lg: 20px;
  --radius-xl: 28px;
  --shadow-sm: 0 1px 3px rgba(0, 0, 0, 0.3);
  --shadow: 0 4px 20px rgba(0, 0, 0, 0.4);
  --shadow-lg: 0 8px 40px rgba(0, 0, 0, 0.5);
  --shadow-glow: 0 0 30px var(--accent-glow);
  --transition: 0.2s cubic-bezier(0.4, 0, 0.2, 1);
  --transition-slow: 0.35s cubic-bezier(0.4, 0, 0.2, 1);
  --max: 1200px;
  --gap: 1.5rem;
  --gap-sm: 0.75rem;
  --gap-lg: 2.5rem;
}

html { scroll-behavior: smooth; scroll-padding-top: 5rem; }

body {
  font-family: system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
  background: var(--bg);
  color: var(--text);
  line-height: 1.6;
  -webkit-font-smoothing: antialiased;
  overflow-x: hidden;
  background-image:
    radial-gradient(ellipse 80% 50% at 50% -20%, rgba(124, 92, 252, 0.08), transparent),
    radial-gradient(ellipse 60% 40% at 80% 80%, rgba(16, 185, 129, 0.05), transparent);
  background-attachment: fixed;
}

a { color: var(--accent-light); text-decoration: none; transition: color var(--transition); }
a:hover { color: var(--accent); }
img, svg { display: block; max-width: 100%; }

/* --- TYPOGRAPHY --- */
h1 { font-size: clamp(2rem, 5vw, 3.5rem); font-weight: 800; letter-spacing: -0.02em; line-height: 1.15; }
h2 { font-size: clamp(1.5rem, 4vw, 2.25rem); font-weight: 700; letter-spacing: -0.01em; }
h3 { font-size: 1.125rem; font-weight: 600; }

.section-title {
  text-align: center;
  margin-bottom: var(--gap-lg);
  background: linear-gradient(135deg, var(--text), var(--text-secondary));
  background-clip: text;
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
}

/* --- BUTTONS --- */
.btn {
  display: inline-flex;
  align-items: center;
  gap: 0.5rem;
  padding: 0.75rem 1.75rem;
  border-radius: 9999px;
  font-weight: 600;
  font-size: 0.9375rem;
  transition: all var(--transition);
  cursor: pointer;
  border: none;
  text-decoration: none;
  white-space: nowrap;
}
.btn-accent {
  background: linear-gradient(135deg, var(--accent), #6c4fe0);
  color: #fff;
  box-shadow: 0 4px 20px var(--accent-glow);
}
.btn-accent:hover { box-shadow: 0 6px 30px var(--accent-strong); transform: translateY(-2px); color: #fff; }
.btn-outline {
  border: 2px solid var(--border-light);
  color: var(--text);
  background: transparent;
}
.btn-outline:hover {
  border-color: var(--accent);
  color: var(--accent-light);
  background: rgba(124, 92, 252, 0.06);
}
.btn-lg { padding: 1rem 2.5rem; font-size: 1.0625rem; }
.btn-sm { padding: 0.5rem 1.25rem; font-size: 0.875rem; }

/* --- PROMO BANNER (conditional showcase via &&) --- */
.promo-banner {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 1rem;
  padding: 0.75rem 1.5rem;
  background: linear-gradient(135deg, var(--accent), #6c4fe0);
  color: #fff;
  font-size: 0.875rem;
  font-weight: 500;
  text-align: center;
  animation: slideDown 0.4s ease;
}
.promo-banner .promo-close {
  background: rgba(255,255,255,0.2);
  border: none;
  color: #fff;
  width: 28px;
  height: 28px;
  border-radius: 50%;
  cursor: pointer;
  font-size: 1.125rem;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: background var(--transition);
  flex-shrink: 0;
}
.promo-banner .promo-close:hover { background: rgba(255,255,255,0.35); }

@keyframes slideDown {
  from { transform: translateY(-100%); opacity: 0; }
  to   { transform: translateY(0);    opacity: 1; }
}

/* --- SITE HEADER (sticky, glass) --- */
.site-header {
  position: sticky;
  top: 0;
  z-index: 100;
  backdrop-filter: blur(16px) saturate(180%);
  -webkit-backdrop-filter: blur(16px) saturate(180%);
  background: rgba(8, 8, 16, 0.8);
  border-bottom: 1px solid var(--border);
}
.site-header nav {
  display: flex;
  align-items: center;
  justify-content: space-between;
  max-width: var(--max);
  margin: 0 auto;
  padding: 1rem 1.5rem;
}
.nav-brand {
  font-weight: 800;
  font-size: 1.25rem;
  letter-spacing: -0.02em;
  background: linear-gradient(135deg, var(--accent-light), var(--accent));
  background-clip: text;
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
}
.nav-links {
  display: flex;
  align-items: center;
  gap: 1.75rem;
  list-style: none;
}
.nav-links a {
  color: var(--text-secondary);
  font-size: 0.9375rem;
  font-weight: 500;
  transition: color var(--transition);
}
.nav-links a:hover { color: var(--text); }

/* --- HERO --- */
.hero {
  max-width: var(--max);
  margin: 0 auto;
  padding: 6rem 1.5rem 5rem;
  text-align: center;
  display: grid;
  gap: var(--gap);
  justify-items: center;
}
.hero-badge {
  display: inline-flex;
  align-items: center;
  gap: 0.5rem;
  padding: 0.375rem 1rem;
  border-radius: 9999px;
  font-size: 0.8125rem;
  font-weight: 600;
  background: rgba(124, 92, 252, 0.1);
  color: var(--accent-light);
  border: 1px solid rgba(124, 92, 252, 0.2);
  animation: fadeInUp 0.6s ease;
}
.hero h1 {
  max-width: 800px;
  background: linear-gradient(135deg, #fff 0%, var(--accent-light) 50%, var(--text-secondary) 100%);
  background-clip: text;
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  animation: fadeInUp 0.6s ease 0.1s both;
}
.hero-subtitle {
  max-width: 620px;
  color: var(--text-secondary);
  font-size: clamp(1rem, 2vw, 1.1875rem);
  line-height: 1.7;
  animation: fadeInUp 0.6s ease 0.2s both;
}
.hero-actions {
  display: flex;
  gap: 1rem;
  flex-wrap: wrap;
  justify-content: center;
  animation: fadeInUp 0.6s ease 0.3s both;
}

@keyframes fadeInUp {
  from { transform: translateY(20px); opacity: 0; }
  to   { transform: translateY(0);    opacity: 1; }
}

/* --- LAYOUT CONTAINER --- */
main section, main article, main aside {
  max-width: var(--max);
  margin: 0 auto;
  padding: 5rem 1.5rem;
}

/* --- FEATURES GRID --- */
.features-section { padding-bottom: 3rem; }
.features-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
  gap: var(--gap);
}
.feature-card {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius-lg);
  padding: 2rem;
  transition: all var(--transition-slow);
  position: relative;
  overflow: hidden;
}
.feature-card::before {
  content: "";
  position: absolute;
  inset: 0;
  border-radius: inherit;
  background: linear-gradient(135deg, var(--accent-glow), transparent);
  opacity: 0;
  transition: opacity var(--transition-slow);
}
.feature-card:hover {
  transform: translateY(-4px);
  border-color: var(--border-light);
  box-shadow: var(--shadow-lg), var(--shadow-glow);
}
.feature-card:hover::before { opacity: 0.5; }
.feature-card.featured {
  border-color: rgba(124, 92, 252, 0.3);
  background: linear-gradient(135deg, rgba(124, 92, 252, 0.06), var(--surface));
}
.feature-icon {
  font-size: 2rem;
  display: block;
  margin-bottom: 1rem;
  position: relative;
  z-index: 1;
}
.feature-card h3 { margin-bottom: 0.5rem; position: relative; z-index: 1; }
.feature-card p {
  color: var(--text-secondary);
  font-size: 0.9375rem;
  line-height: 1.65;
  position: relative;
  z-index: 1;
}
.features-grid .feature-card:nth-child(odd)  { border-left: 3px solid var(--accent); }
.features-grid .feature-card:nth-child(even) { border-left: 3px solid var(--green); }

/* --- STATS GRID --- */
.stats-section { margin: 2rem auto; padding: 0 1.5rem; }
.stats-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: var(--gap);
  background: linear-gradient(135deg, var(--surface), var(--surface-raised));
  border: 1px solid var(--border);
  border-radius: var(--radius-xl);
  padding: 3rem 2rem;
}
.stat-card { text-align: center; padding: 1rem; }
.stat-number {
  display: block;
  font-size: clamp(1.75rem, 4vw, 2.5rem);
  font-weight: 800;
  letter-spacing: -0.02em;
  background: linear-gradient(135deg, var(--accent-light), var(--green));
  background-clip: text;
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
}
.stat-label {
  display: block;
  color: var(--text-muted);
  font-size: 0.875rem;
  font-weight: 500;
  margin-top: 0.25rem;
  text-transform: uppercase;
  letter-spacing: 0.05em;
}
@media (min-width: 768px) { .stats-grid { grid-template-columns: repeat(4, 1fr); } }

/* --- PRICING GRID --- */
.pricing-toggle {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 0.75rem;
  margin-bottom: 2.5rem;
  font-size: 0.9375rem;
  color: var(--text-secondary);
}
.pricing-period {
  font-weight: 600;
  padding: 0.25rem 0.75rem;
  border-radius: 9999px;
  transition: all var(--transition);
}
.pricing-period.active { color: var(--text); background: rgba(124, 92, 252, 0.12); }
.pricing-period:not(.active) { color: var(--text-muted); }
.pricing-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
  gap: var(--gap);
  align-items: start;
}
.pricing-card {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius-lg);
  padding: 2.25rem 2rem;
  transition: all var(--transition-slow);
  position: relative;
  display: flex;
  flex-direction: column;
  gap: 1rem;
}
.pricing-card:hover { transform: translateY(-4px); box-shadow: var(--shadow-lg); }
.pricing-card.popular {
  border-color: var(--accent);
  background: linear-gradient(180deg, rgba(124, 92, 252, 0.08), var(--surface));
  box-shadow: 0 4px 30px var(--accent-glow);
  transform: scale(1.03);
}
.pricing-card.popular:hover { transform: scale(1.03) translateY(-4px); }
.popular-badge {
  position: absolute;
  top: -12px;
  left: 50%;
  transform: translateX(-50%);
  padding: 0.25rem 1.25rem;
  border-radius: 9999px;
  font-size: 0.75rem;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.05em;
  background: linear-gradient(135deg, var(--accent), #6c4fe0);
  color: #fff;
  box-shadow: 0 4px 16px var(--accent-strong);
}
.pricing-card h3 { font-size: 1.25rem; }
.pricing-amount {
  font-size: 2.5rem;
  font-weight: 800;
  letter-spacing: -0.02em;
}
.pricing-desc { color: var(--text-muted); font-size: 0.9375rem; }
.pricing-features {
  list-style: none;
  display: flex;
  flex-direction: column;
  gap: 0.625rem;
  flex: 1;
}
.pricing-features li { font-size: 0.9375rem; }
.pricing-features li.included { color: var(--text); }
.pricing-features li.excluded { color: var(--text-muted); text-decoration: line-through; }
.pricing-card .btn { margin-top: auto; text-align: center; justify-content: center; width: 100%; }

/* --- TESTIMONIALS GRID --- */
.testimonials-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(340px, 1fr));
  gap: var(--gap);
}
.testimonial-card {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius-lg);
  padding: 2rem;
  transition: all var(--transition-slow);
  position: relative;
}
.testimonial-card::before {
  content: "\201C";
  position: absolute;
  top: 0.5rem;
  left: 1.25rem;
  font-size: 5rem;
  line-height: 1;
  color: var(--accent);
  opacity: 0.12;
  font-family: Georgia, serif;
  pointer-events: none;
}
.testimonial-card:hover { border-color: var(--border-light); box-shadow: var(--shadow); }
.testimonial-card blockquote {
  font-size: 0.9375rem;
  line-height: 1.7;
  color: var(--text-secondary);
  font-style: italic;
  margin-bottom: 1.5rem;
  position: relative;
  z-index: 1;
}
.testimonial-card footer {
  display: flex;
  align-items: center;
  gap: 0.75rem;
}
.testimonial-card footer strong { display: block; font-size: 0.9375rem; font-weight: 600; }
.testimonial-card footer span   { display: block; font-size: 0.8125rem; color: var(--text-muted); }

/* --- FAQ GRID --- */
.faq-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(400px, 1fr));
  gap: var(--gap-sm);
}
.faq-item {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  transition: all var(--transition);
}
.faq-item:hover { border-color: var(--border-light); }
.faq-item[open] { border-color: var(--accent); box-shadow: 0 0 20px var(--accent-glow); }
.faq-item summary {
  padding: 1.25rem 1.5rem;
  font-weight: 600;
  font-size: 0.9375rem;
  cursor: pointer;
  list-style: none;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 1rem;
}
.faq-item summary::-webkit-details-marker { display: none; }
.faq-item summary::after {
  content: "+";
  font-size: 1.25rem;
  color: var(--accent-light);
  transition: transform var(--transition);
  flex-shrink: 0;
}
.faq-item[open] summary::after { content: "\2212"; transform: rotate(180deg); }
.faq-item p {
  padding: 0 1.5rem 1.25rem;
  color: var(--text-secondary);
  font-size: 0.9375rem;
  line-height: 1.7;
}

/* --- CTA SECTION --- */
.cta-section {
  background: linear-gradient(135deg, rgba(124, 92, 252, 0.1), rgba(16, 185, 129, 0.05));
  border: 1px solid var(--border);
  border-radius: var(--radius-xl);
  text-align: center;
  display: grid;
  gap: 1rem;
  justify-items: center;
  padding: 4rem 2rem !important;
  margin: 3rem auto;
  max-width: var(--max);
}
.cta-section p { color: var(--text-secondary); font-size: 1.0625rem; max-width: 500px; }

/* --- FOOTER GRID --- */
.site-footer { border-top: 1px solid var(--border); padding: 4rem 1.5rem 2rem; margin-top: 2rem; }
.footer-grid {
  max-width: var(--max);
  margin: 0 auto;
  display: grid;
  grid-template-columns: 2fr repeat(3, 1fr);
  gap: var(--gap-lg);
}
.footer-brand p {
  color: var(--text-muted);
  font-size: 0.875rem;
  margin-top: 0.75rem;
  max-width: 280px;
}
.footer-column h4 {
  font-size: 0.8125rem;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: var(--text-muted);
  margin-bottom: 1rem;
}
.footer-column ul { list-style: none; display: flex; flex-direction: column; gap: 0.625rem; }
.footer-column a { color: var(--text-secondary); font-size: 0.9375rem; transition: color var(--transition); }
.footer-column a:hover { color: var(--text); }
.footer-bottom {
  max-width: var(--max);
  margin: 3rem auto 0;
  padding-top: 1.5rem;
  border-top: 1px solid var(--border);
  display: flex;
  align-items: center;
  justify-content: space-between;
  flex-wrap: wrap;
  gap: 0.75rem;
  font-size: 0.8125rem;
  color: var(--text-muted);
}

/* --- RESPONSIVE BREAKPOINTS --- */
@media (max-width: 1024px) { .footer-grid { grid-template-columns: repeat(2, 1fr); } }
@media (max-width: 768px) {
  .features-grid      { grid-template-columns: 1fr; }
  .testimonials-grid   { grid-template-columns: 1fr; }
  .faq-grid           { grid-template-columns: 1fr; }
  .hero { padding: 4rem 1.5rem 3rem; }
  main section, main article, main aside { padding: 3rem 1.5rem; }
  .nav-links { display: none; }
  .footer-grid { grid-template-columns: 1fr; gap: var(--gap); }
  .footer-brand p { max-width: 100%; }
}
@media (max-width: 480px) {
  .pricing-grid  { grid-template-columns: 1fr; }
  .stats-grid    { grid-template-columns: 1fr; }
  .hero-actions  { flex-direction: column; width: 100%; }
  .hero-actions .btn { width: 100%; justify-content: center; }
  .pricing-card.popular { transform: none; }
  .pricing-card.popular:hover { transform: translateY(-4px); }
}

/* --- ACCESSIBLE FOCUS --- */
:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; border-radius: 4px; }

/* --- REDUCED MOTION --- */
@media (prefers-reduced-motion: reduce) {
  *, *::before, *::after { animation-duration: 0.01ms !important; transition-duration: 0.01ms !important; }
}
  </style>
</head>
<body>
  {{ showBanner && <aside class="promo-banner">
    <span>Limited-time offer — 50% off annual plans for new customers!</span>
    <button class="promo-close" onclick="this.parentElement.remove()" aria-label="Dismiss">&times;</button>
  </aside> }}

  <header class="site-header">
    <nav>
      <a href="/" class="nav-brand">Bialet</a>
      <ul class="nav-links">
        <li><a href="#features">Features</a></li>
        <li><a href="#pricing">Pricing</a></li>
        <li><a href="#testimonials">Testimonials</a></li>
        <li><a href="#faq">FAQ</a></li>
      </ul>
      <a href="#" class="btn btn-sm btn-accent">Get Started</a>
    </nav>
  </header>

  <main>
    <section class="hero">
      <span class="hero-badge">✨ Now in public beta</span>
      <h1>{{ heroTitle.safe }}</h1>
      <p class="hero-subtitle">{{ heroSubtitle.safe }}</p>
      <div class="hero-actions">
        <a href="#" class="btn btn-accent btn-lg">Start free trial</a>
        <a href="#features" class="btn btn-outline btn-lg">See how it works</a>
      </div>
    </section>

    <section class="features-section" id="features">
      <h2 class="section-title">Everything you need to ship</h2>
      <div class="features-grid">
        {{ features.map{|f| <article class="feature-card {{ f["highlight"] && "featured" }}">
          <span class="feature-icon" aria-hidden="true">{{ f["icon"] }}</span>
          <h3>{{ f["title"].safe }}</h3>
          <p>{{ f["desc"].safe }}</p>
        </article> } }}
      </div>
    </section>

    <article class="stats-section">
      <div class="stats-grid">
        {{ stats.map{|s| <div class="stat-card">
          <span class="stat-number">{{ s["value"].safe }}</span>
          <span class="stat-label">{{ s["label"].safe }}</span>
        </div> } }}
      </div>
    </article>

    <section class="pricing-section" id="pricing">
      <h2 class="section-title">Simple, transparent pricing</h2>
      <div class="pricing-toggle">
        <span class="pricing-period {{ isAnnual ? "" : "active" }}">Monthly</span>
        <span aria-hidden="true">&middot;</span>
        <span class="pricing-period {{ isAnnual ? "active" : "" }}">Annual {{ isAnnual && "— save 17\x25" }}</span>
      </div>
      <div class="pricing-grid">
        {{ pricing.map{|p| <article class="pricing-card {{ p["popular"] && "popular" }}">
          {{ p["popular"] && <span class="popular-badge">Most Popular</span> }}
          <h3>{{ p["name"].safe }}</h3>
          <p class="pricing-amount">{{ isAnnual ? p["annual"].safe : p["monthly"].safe }}<span style="font-size:1rem;font-weight:400;color:var(--text-muted)">/mo</span></p>
          <p class="pricing-desc">{{ p["desc"].safe }}</p>
          <ul class="pricing-features">
            {{ p["features"].map{|feat| <li class="{{ feat["included"] ? "included" : "excluded" }}">
              <span aria-hidden="true">{{ feat["included"] ? "✓" : "✕" }}</span> {{ feat["text"].safe }}
            </li> } }}
          </ul>
          <a href="#" class="btn {{ p["popular"] ? "btn-accent" : "btn-outline" }}">{{ p["popular"] ? "Start Pro trial" : "Get started" }}</a>
        </article> } }}
      </div>
    </section>

    <aside class="testimonials-section" id="testimonials">
      <h2 class="section-title">Loved by teams worldwide</h2>
      <div class="testimonials-grid">
        {{ testimonials.map{|t| <article class="testimonial-card">
          <blockquote>{{ t["quote"].safe }}</blockquote>
          <footer>
            <div>
              <strong>{{ t["name"].safe }}</strong>
              <span>{{ t["role"].safe }}</span>
            </div>
          </footer>
        </article> } }}
      </div>
    </aside>

    <section class="faq-section" id="faq">
      <h2 class="section-title">Frequently asked questions</h2>
      <div class="faq-grid">
        {{ faqs.map{|q| <details class="faq-item">
          <summary>{{ q["q"].safe }}</summary>
          <p>{{ q["a"].safe }}</p>
        </details> } }}
      </div>
    </section>

    <footer class="cta-section">
      <h2>Ready to ship faster?</h2>
      <p>Start building with a single binary. No config files, no boilerplate, no nonsense.</p>
      <a href="#" class="btn btn-accent btn-lg">Start free trial &rarr;</a>
    </footer>
  </main>

  <footer class="site-footer">
    <div class="footer-grid">
      <div class="footer-brand">
        <a href="/" class="nav-brand">Bialet</a>
        <p>A modern framework for people who build things. Single binary, zero config, infinite possibilities.</p>
      </div>
      {{ footerLinks.map{|col| <div class="footer-column">
        <h4>{{ col["title"].safe }}</h4>
        <ul>
          {{ col["items"].map{|link| <li><a href="{{ link["href"] }}">{{ link["label"].safe }}</a></li> } }}
        </ul>
      </div> } }}
    </div>
    <div class="footer-bottom">
      <span>&copy; 2026 Bialet. All rights reserved.</span>
      <span>Built with <a href="https://bialet.dev">Bialet</a> &mdash; CSS Grid showcase</span>
    </div>
  </footer>
</body>
</html>
