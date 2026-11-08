#!/bin/sh
# Deterministic end-to-end test for HTTPS requester, parsers, cache and errors.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TEMP=$(mktemp -d)
trap 'rm -rf "$TEMP"' EXIT HUP INT TERM
mkdir -p "$TEMP/bin" "$TEMP/data"
cat > "$TEMP/bin/curl" <<'MOCK'
#!/bin/sh
for url do :; done
case "$url" in
  *bcdata.sgs.432*) printf '%s\n' '[{"data":"07/11/2026","valor":"13.75"}]' ;;
  *CotacaoMoedaPeriodo*%27USD%27*) printf '%s\n' '{"value":[{"cotacaoVenda":5.32,"dataHoraCotacao":"2026-11-07 13:10:00.000"}]}' ;;
  *CotacaoMoedaPeriodo*%27EUR%27*) printf '%s\n' '{"value":[{"cotacaoVenda":6.12,"dataHoraCotacao":"2026-11-07 13:10:00.000"}]}' ;;
  *brl.rate.sx*) cat <<'RATESBRL'
│ Rank │ Coin │ Price (BRL) │ Change (24H) │
│ 1    │ BTC  │ 425600.0    │ -1.0%        │
│ 12   │ XMR  │ 2660.0      │ +0.5%        │
RATESBRL
    ;;
  *rate.sx*) cat <<'RATES'
│ Rank │ Coin │ Price (USD) │ Change (24H) │
│ 1    │ BTC  │ 80000.0     │ -1.0%        │
│ 12   │ XMR  │ 500.0       │ +0.5%        │
RATES
    ;;
  *) exit 22 ;;
esac
MOCK
chmod +x "$TEMP/bin/curl"
PATH="$TEMP/bin:$PATH" XDG_DATA_HOME="$TEMP/data" "$ROOT/invest-market-check" > "$TEMP/good.txt"
grep -q 'Atualizados nesta tentativa: 5 de 5' "$TEMP/good.txt"
grep -q 'INVEST_MARKET' "$TEMP/data/invest/mercado.tsv"
[ "$(grep -c '^\([0-4]\)' "$TEMP/data/invest/mercado.tsv")" -eq 5 ]
# Direct BRL succeeds even if BCB USD fails, without relying on FX conversion.
cat > "$TEMP/bin/curl" <<'MOCK'
#!/bin/sh
for url do :; done
case "$url" in
  *bcdata.sgs.432*) printf '%s\n' '[{"data":"07/11/2026","valor":"13.75"}]' ;;
  *CotacaoMoedaPeriodo*%27USD%27*) exit 22 ;;
  *CotacaoMoedaPeriodo*%27EUR%27*) exit 22 ;;
  *brl.rate.sx*) cat <<'RATESBRL'
│ Rank │ Coin │ Price (BRL) │
│ 1    │ BTC  │ 425600.0    │
│ 11   │ XMR  │ 2660.0      │
RATESBRL
    ;;
  *rate.sx*) echo 'Unexpected USD fallback' >&2; exit 50 ;;
  *) exit 22 ;;
esac
MOCK
chmod +x "$TEMP/bin/curl"
PATH="$TEMP/bin:$PATH" XDG_DATA_HOME="$TEMP/data" "$ROOT/invest-market-check" > "$TEMP/brl.txt"
grep -q 'Atualizados nesta tentativa: 3 de 5' "$TEMP/brl.txt"
# Restore full online mock to keep subsequent tests deterministic.
cat > "$TEMP/bin/curl" <<'MOCK'
#!/bin/sh
for url do :; done
case "$url" in
  *bcdata.sgs.432*) printf '%s\n' '[{"data":"07/11/2026","valor":"13.75"}]' ;;
  *CotacaoMoedaPeriodo*%27USD%27*) printf '%s\n' '{"value":[{"cotacaoVenda":5.32,"dataHoraCotacao":"2026-11-07 13:10:00.000"}]}' ;;
  *CotacaoMoedaPeriodo*%27EUR%27*) printf '%s\n' '{"value":[{"cotacaoVenda":6.12,"dataHoraCotacao":"2026-11-07 13:10:00.000"}]}' ;;
  *brl.rate.sx*) exit 22 ;;
  *rate.sx*) cat <<'RATES'
│ Rank │ Coin │ Price (USD) │
│ 1    │ BTC  │ 80000.0     │
│ 11   │ XMR  │ 500.0       │
RATES
    ;;
  *) exit 22 ;;
esac
MOCK
chmod +x "$TEMP/bin/curl"
PATH="$TEMP/bin:$PATH" XDG_DATA_HOME="$TEMP/data" "$ROOT/invest-market-check" > "$TEMP/usd.txt"
grep -q 'Atualizados nesta tentativa: 5 de 5' "$TEMP/usd.txt"
# Inject failure for an unavailable DNS resolver; ensure prior quotes survive.
cat > "$TEMP/bin/curl" <<'MOCK'
#!/bin/sh
exit 6
MOCK
chmod +x "$TEMP/bin/curl"
if PATH="$TEMP/bin:$PATH" XDG_DATA_HOME="$TEMP/data" "$ROOT/invest-market-check" > "$TEMP/bad.txt"; then
  echo 'Expected nonzero exit on full outage' >&2; exit 1
fi
grep -q 'DNS: servidor não encontrado' "$TEMP/bad.txt"
grep -q 'Atualizados nesta tentativa: 0 de 5' "$TEMP/bad.txt"
[ "$(grep -c '^\([0-4]\)' "$TEMP/data/invest/mercado.tsv")" -eq 5 ]
# A partial success must save valid data instead of discarding all results.
cat > "$TEMP/bin/curl" <<'MOCK'
#!/bin/sh
for url do :; done
case "$url" in
  *bcdata.sgs.432*) printf '%s\n' '[{"data":"07/11/2026","valor":"14.00"}]' ;;
  *) exit 22 ;;
esac
MOCK
chmod +x "$TEMP/bin/curl"
PATH="$TEMP/bin:$PATH" XDG_DATA_HOME="$TEMP/data" "$ROOT/invest-market-check" > "$TEMP/partial.txt"
grep -q 'Atualizados nesta tentativa: 1 de 5' "$TEMP/partial.txt"
grep -q '14.0000000000' "$TEMP/data/invest/mercado.tsv"
grep -q '425600.0000000000' "$TEMP/data/invest/mercado.tsv"
# When only USD fails, rate.sx data must not be relabeled as BRL.
cat > "$TEMP/bin/curl" <<'MOCK'
#!/bin/sh
for url do :; done
case "$url" in
  *bcdata.sgs.432*) printf '%s\n' '[{"data":"07/11/2026","valor":"14.75"}]' ;;
  *CotacaoMoedaPeriodo*%27EUR%27*) printf '%s\n' '{"value":[{"cotacaoVenda":6.12,"dataHoraCotacao":"2026-11-07 13:10:00.000"}]}' ;;
  *CotacaoMoedaPeriodo*%27USD%27*) exit 22 ;;
  *brl.rate.sx*) exit 22 ;;
  *rate.sx*) echo 'Unexpected USD fallback without fresh USD/BRL' >&2; exit 50 ;;
  *) exit 22 ;;
esac
MOCK
chmod +x "$TEMP/bin/curl"
PATH="$TEMP/bin:$PATH" XDG_DATA_HOME="$TEMP/data" "$ROOT/invest-market-check" > "$TEMP/fx.txt"
grep -q 'HTTP: API recusou' "$TEMP/fx.txt"
grep -q 'Atualizados nesta tentativa: 2 de 5' "$TEMP/fx.txt"
# USD and previously saved BTC and XMR are not silently replaced.
grep -q '425600.0000000000' "$TEMP/data/invest/mercado.tsv"
echo 'network mock: rate.sx USD conversion, DNS outage, partial updates, and cache preserved OK' 
