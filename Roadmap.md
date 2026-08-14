# Roadmap — מ"עובד מקומית" ל"שירות ענן מוכח"

מסמך זה ממזג שני סבבי פידבק (הצעת ה-6 רכיבים, ואז ההכוונה "תהפכי את זה לשירות ענני")
עם המצב בפועל של הריפו: [`Server_Design.md`](Server_Design.md) (החישוב — למה תהליך יחיד
לא מספיק ב-100M/10M), ו-[`Microservices_Design.md`](Microservices_Design.md) (חלוקה ל-6
רכיבים, טבלת "בנוי מול עתידי").

**עיקרון מארגן:** הפידבק השני משנה סדר עדיפויות — לא לקפוץ ישר ל-microservices, אלא קודם
להפוך את `kfc_server` הקיים לשירות production אמיתי בענן (wss/TLS). זה עדיין לא קיים היום
ולפי ההערכה זה הצעד הכי משתלם. מה שכבר נבנה (`api-gateway/` Java+Postgres) לא זורקים —
הוא נשאר כתוספת מקבילה, פשוט לא בראש התור.

---

## שלב 0 — לשחרר את החסימה הנוכחית ✅ הושלם

`docker compose up --build` נתקע בהורדת `postgres:16` (`unexpected EOF`) — תוקן, נבדק שוב
בהצלחה. **שינוי כיוון בדרך:** ה-API Gateway הוחלף מ-Java/Postgres ל-C++ (endpoints ישירות
ב-`kfc_server` על פורט 8081) — פחות polyglot, חשבון משתמשים אחד במקום שניים. `postgres`
ו-`api-gateway/` הוסרו לגמרי מה-compose ומהריפו.

## שלב 1 — kfc_server בענן, אמיתי (הערך הגבוה ביותר)

- ⏳ שכירת מכונת Linux אצל ספק ענן, Docker עליה, `docker compose up` — **התשתית מוכנה**
  ב-[`DOCKER/`](DOCKER) (Caddyfile + compose.prod.yaml + הוראות), אבל לא בוצע בפועל (דורש
  חשבון/תשלום/דומיין אמיתיים).
- ✅ Nginx/Caddy כ-reverse proxy: `wss://` + TLS אוטומטי, פתוח רק 80/443 — `DOCKER/Caddyfile`.
- ✅ מגבלת גודל הודעה (כבר הייתה קיימת) + מגבלת הודעות/שנייה (`kMaxMessagesPerSecond`).
- ✅ timeout ללקוח לא מגיב (ping/pong של ixwebsocket), graceful shutdown (SIGINT/SIGTERM),
  health check endpoint (`GET /health`), log rotation (`FileLogger`).
- ✅ הסיסמה לא עוברת כפרמטר שורת פקודה — `kfc_gui_app` שואל אותה בפרומפט.
- ❌ גרסת Release להורדה של `kfc_gui_app` — עדיין לא.
- ❌ **מבחן קבלה:** שני מחשבים שונים, דרך האינטרנט, משחקים זה מול זה — דורש פריסה אמיתית.

## שלב 2 — הארכיטקטורה הקטנה

לפי `Microservices_Design.md` המעודכן — register/login/history כבר מקופלים לתוך
`kfc_server` עצמו (לא Java/Postgres נפרדים). חסר:

- **Redis** — ניתוב `room_id → worker` (ולא NATS, מיותר בשלב הזה).
- **Game Worker שני** — היום יש מופע יחיד של `kfc_server`; צריך שני תהליכים + ניתוב שחקן/צופה
  לחדר הנכון דרך Redis, לא רק "כולם מתחברים לאותו port".
- מבחן קבלה: שני workers רצים, שחקנים וצופים מגיעים לחדר הנכון על פני שני התהליכים.

## שלב 3 — Observability + Load Test

מדדים לחשוף (`/metrics`, `/health` בכל שירות): חיבורי WebSocket פעילים, משחקים פעילים,
הודעות/שנייה, זמן טיפול ממוצע במהלך, CPU/זיכרון, הודעות שנדחו, התנתקויות, משך tick
ממוצע/מקסימלי, משחקים ל-worker thread. Prometheus + Grafana. אחר כך load test אמיתי
(k6/Locust): 10,000 לקוחות מדומים, 5,000 משחקים במקביל, ופרסום מספרים אמיתיים —
moves/sec, P95 latency, זיכרון.

## שלב 4 — תכונה ייחודית אחת (לבחור אחת, לא הכל)

- **בוט בזמן אמת** — הכי מעניין אלגוריתמית (cooldown, תנועות מקבילות).
- **דירוג/טורנירים/צפייה** — ELO, היסטוריה, matchmaking גיאוגרפי.
- **Replay** — לפי [`CODE_REVIEW.md`](../CODE_REVIEW.md) §4, `ArrivalEvents` כבר קיים
  ומשודר ל-observers — זו הדרך הזולה ביותר מבין השלוש להתחיל ממנה.

## שלב 5 — חיזוק איכות קוד (בונה על 405+ הבדיקות הקיימות)

Code coverage, AddressSanitizer, **ThreadSanitizer** (חשוב במיוחד — `MatchScheduler`
רב-thread), clang-tidy, fuzzing למפענח ה-JSON, load test ל-`MatchScheduler`, בדיקות
ניתוק/חיבור מחדש תחת תחרותיות, בדיקות קלט עוין, CI שבונה Debug+Release.

## שלב 6 — ליטוש ההצגה

סרטון 2–3 דקות, צילום של שני שחקנים+צופה, תרשים ארכיטקטורה (יש כבר ב-`Server_Design.md`/
`Microservices_Design.md`), תרשים זרימת מהלך, הפעלה בפקודה אחת, תוצאות load test, CI ירוק,
רשימת החלטות ארכיטקטוניות (יש כבר), מסמך מגבלות ידועות.

---

## סדר עבודה מומלץ

0. לתקן את ה-build התקוע → 1. פריסת `kfc_server` לענן עם wss/TLS → 2. Redis + worker שני
→ 3. metrics + load test → 4. סרטון ותיעוד → 5. **רק אז** לבחור בין בוט/Replay/טורנירים
→ 6. חיזוק איכות קוד (אפשר במקביל לאורך כל הדרך).
