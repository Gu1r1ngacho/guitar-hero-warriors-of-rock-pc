#include "generated/re_wor_config.h"
#include "generated/re_wor_init.h"

#include "discord_rpc.h"

#include <rex/rex_app.h>
#include <rex/ppc.h>
#include <rex/runtime.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/audio/sdl/sdl_audio_system.h>
#include <rex/input/input_system.h>
#include <rex/ui/keybinds.h>

#include <cstdio>
#include <unordered_set>
#include <cstring>
#include <unordered_map>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>
#include <cmath>
#include <mutex>
#include <vector>
#ifdef REWOR_DEV
#include "stub_tracker.h"
#endif

#ifdef _WIN32
#include <windows.h>
#include <rex/ui/window_win.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

#ifdef REWOR_DEV
static std::ofstream& dbg() {
  static std::ofstream log("re_wor_trace.log", std::ios::out | std::ios::trunc);
  return log;
}
#endif
#ifdef REWOR_DEV
#define TRACE(msg) do { dbg() << msg << std::endl; dbg().flush(); } while(0)
#else
#define TRACE(msg) ((void)0)
#endif

// Parche: escribir modulo, desplazamiento y nombre de funcion de cada direccion
static void LogAddrInfo(std::ofstream& log, uintptr_t addr) {
  HMODULE mod = nullptr;
  if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCSTR>(addr), &mod)) {
    char name[MAX_PATH] = {};
    GetModuleFileNameA(mod, name, MAX_PATH);
    std::string path(name);
    log << "  " << path.substr(path.find_last_of('\\') + 1) << " +0x" << std::hex
        << (addr - reinterpret_cast<uintptr_t>(mod));
  }
  static bool sym_ready = [] {
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    return SymInitialize(GetCurrentProcess(), nullptr, TRUE) != FALSE;
  }();
  alignas(SYMBOL_INFO) char buf[sizeof(SYMBOL_INFO) + 256] = {};
  auto* sym = reinterpret_cast<SYMBOL_INFO*>(buf);
  sym->SizeOfStruct = sizeof(SYMBOL_INFO);
  sym->MaxNameLen = 255;
  DWORD64 disp = 0;
  if (sym_ready && SymFromAddr(GetCurrentProcess(), addr, &disp, sym)) {
    log << "  -> " << sym->Name << " +0x" << std::hex << disp;
  }
}

static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ep) {
  std::ofstream log("re_wor_crash_detail.log", std::ios::out | std::ios::trunc);
  log << "Exception: 0x" << std::hex << ep->ExceptionRecord->ExceptionCode << "\n";
  uintptr_t rip = ep->ContextRecord->Rip;
  log << "RIP: 0x" << std::hex << rip;
  LogAddrInfo(log, rip);
  log << "\n";
  if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
    log << "Access: " << (ep->ExceptionRecord->ExceptionInformation[0] == 0 ? "READ" :
                          ep->ExceptionRecord->ExceptionInformation[0] == 1 ? "WRITE" : "EXEC") << "\n";
    log << "Target: 0x" << std::hex << ep->ExceptionRecord->ExceptionInformation[1] << "\n";
  }
  void* stack[64];
  USHORT frames = CaptureStackBackTrace(0, 64, stack, NULL);
  log << "\nStack (" << std::dec << frames << " frames):\n";
  for (USHORT i = 0; i < frames; i++) {
    uintptr_t a = reinterpret_cast<uintptr_t>(stack[i]);
    log << "  [" << std::dec << i << "] 0x" << std::hex << a;
    LogAddrInfo(log, a);
    log << "\n";
  }
  log.flush();
  return EXCEPTION_CONTINUE_SEARCH;
}
#endif

// ---------------------------------------------------------------------------
// Safe CRT init ctor caller — uses SEH to catch crashes in individual ctors
// ---------------------------------------------------------------------------
static void safe_call_ctor(PPCContext& ctx, uint8_t* base, uint32_t ppc_addr,
                           const char* source, int& called, int& skipped, int& crashed) {
  auto* fn = PPC_LOOKUP_FUNC(base, ppc_addr);
  if (!fn) {
    skipped++;
    return;
  }
#ifdef _WIN32
  __try {
    fn(ctx, base);
    called++;
  } __except(EXCEPTION_EXECUTE_HANDLER) {
    TRACE("[ctor] CRASH in 0x" << std::hex << ppc_addr << " (" << source 
          << ") exception=0x" << GetExceptionCode() << " - skipping");
    crashed++;
  }
#else
  fn(ctx, base);
  called++;
#endif
}

// ---------------------------------------------------------------------------
// Hook: sub_822CCB60 — CRT static initializer callback iterator (SAFE VERSION)
// ---------------------------------------------------------------------------
extern "C" PPC_FUNC(sub_822CCB60) {
  TRACE("[sub_822CCB60] Safe CRT init started");
  int called = 0, skipped = 0, crashed = 0;

  // Stage 1: Single callback at 0x82064074
  uint32_t fn_addr = 0;
  bool stage1_readable = false;
#ifdef _WIN32
  __try {
    fn_addr = PPC_LOAD_U32(0x82064074);
    stage1_readable = true;
  } __except(EXCEPTION_EXECUTE_HANDLER) {
    TRACE("[sub_822CCB60] Cannot read stage1 addr 0x82064074 (exc=0x" 
          << std::hex << GetExceptionCode() << ") - skipping stage1");
  }
#else
  fn_addr = PPC_LOAD_U32(0x82064074);
  stage1_readable = true;
#endif
  if (stage1_readable && fn_addr != 0) {
    safe_call_ctor(ctx, base, fn_addr, "stage1", called, skipped, crashed);
  }

  // Stage 2: Table 1 (0x827D2720 to 0x827D272C)
  TRACE("[sub_822CCB60] Stage 2: table1 (0x827D2720-0x827D272C)");
  for (uint32_t ea = 0x827D2720; ea < 0x827D272C; ea += 4) {
    fn_addr = PPC_LOAD_U32(ea);
    if (fn_addr != 0) {
      safe_call_ctor(ctx, base, fn_addr, "table1", called, skipped, crashed);
    }
  }

  // Stage 3: Table 2 (0x827D0414 to 0x827D271C)  
  TRACE("[sub_822CCB60] Stage 3: table2 (0x827D0414-0x827D271C)");
  for (uint32_t ea = 0x827D0414; ea < 0x827D271C; ea += 4) {
    fn_addr = PPC_LOAD_U32(ea);
    if (fn_addr != 0 && fn_addr != 0xFFFFFFFF) {
      safe_call_ctor(ctx, base, fn_addr, "table2", called, skipped, crashed);
    }
  }

  TRACE("[sub_822CCB60] Done: called=" << std::dec << called 
        << " skipped=" << skipped << " crashed=" << crashed);
}

// ---------------------------------------------------------------------------
// Parche: sub_826F7390 reubica la direccion de memoria de una textura
// (campos +0x20 y +0x30 de su encabezado). A veces el juego la llama con una
// textura nula (r3 = 0) y crashea al leer la direccion 0x20.
// Si la textura es nula no hay nada que reubicar: salimos sin hacer nada.
// ---------------------------------------------------------------------------
extern "C" PPC_FUNC(__imp__sub_826F7390);  // la funcion original del juego

extern "C" PPC_FUNC(sub_826F7390) {
  if (ctx.r3.u32 == 0) {
    TRACE("[Parche] sub_826F7390 con textura nula (llamada desde 0x"
          << std::hex << static_cast<uint32_t>(ctx.lr) << ") - ignorada");
    return;
  }
  __imp__sub_826F7390(ctx, base);
}

// Interruptores de los parches de CPU (se pueden cambiar sin recompilar, ej.
// --cpu_patch_workers=false). Ver los parches mas abajo.
REXCVAR_DEFINE_BOOL(cpu_patch_render, true, "ReWor", "Hilo de dibujo: dormir 1 ms al esperar cuadro");
REXCVAR_DEFINE_BOOL(cpu_patch_workers, true, "ReWor", "Hilos trabajadores: esperar 1 ms en vez de 0");
REXCVAR_DEFINE_BOOL(cpu_patch_main, false, "ReWor",
                    "Hilo principal: dormir tras 0,5 ms esperando a la GPU (baja FPS)");

// ---------------------------------------------------------------------------
// Parche: el hilo principal (sub_8242C450) espera en un bucle a que el campo
// +136 del objeto 0x82A81CF8 llegue a 0 (probablemente "trabajo pendiente de
// la GPU"). Si ese aviso nunca llega, el juego se queda congelado.
// Si la espera dura mas de 2 segundos, la cortamos poniendo el campo en 0.
// ---------------------------------------------------------------------------
extern "C" PPC_FUNC(__imp__sub_825528E0);  // la funcion original: r3 = campo +136

extern "C" PPC_FUNC(sub_825528E0) {
  const uint32_t obj = ctx.r3.u32;
  const uint32_t caller = static_cast<uint32_t>(ctx.lr);
  __imp__sub_825528E0(ctx, base);

  if (caller != 0x8242C51C) return;  // solo nos interesa el bucle del hilo principal

  static std::chrono::steady_clock::time_point wait_start{};
  static bool waiting = false;
  if (ctx.r3.u32 == 0) {  // la espera termino normalmente
    waiting = false;
    return;
  }
  // Parche de rendimiento: mientras espera a la GPU, ceder el turno del procesador
  // para que el hilo de la GPU (y los demas) puedan avanzar, en vez de girar en vacio.
  const auto now = std::chrono::steady_clock::now();
  if (!waiting) {
    waiting = true;
    wait_start = now;
    std::this_thread::yield();
    return;
  }
  // Parche de CPU: si la espera ya pasa de 0,5 ms, dormir 1 ms en vez de girar.
  // Desactivado por defecto: bajaba los FPS y desfasaba las notas.
  if (REXCVAR_GET(cpu_patch_main) && now - wait_start > std::chrono::microseconds(500)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } else {
    std::this_thread::yield();
  }
  if (now - wait_start > std::chrono::seconds(2)) {
    TRACE("[Parche] espera atascada en 0x" << std::hex << (obj + 136)
          << " (valor " << std::dec << ctx.r3.u32 << ") - forzando a 0");
    PPC_STORE_U32(obj + 136, 0);
    ctx.r3.u64 = 0;
    waiting = false;
  }
}

// ---------------------------------------------------------------------------
// Parche de CPU: el hilo de dibujo (sub_825840F0) espera el siguiente cuadro con
//   while (sub_8239FDC0() < objetivo) {}   // objetivo = campo +7404 de r30
// girando sin descanso (un nucleo entero al 100%). sub_8239FDC0 solo lee un
// contador de cuadros (0x8298C1F0). Si todavia no llego, dormimos 1 ms.
// ---------------------------------------------------------------------------
extern "C" PPC_FUNC(__imp__sub_8239FDC0);  // la funcion original: r3 = contador

extern "C" PPC_FUNC(sub_8239FDC0) {
  __imp__sub_8239FDC0(ctx, base);
  if (!REXCVAR_GET(cpu_patch_render) || static_cast<uint32_t>(ctx.lr) != 0x825841AC) return;

  // El contador avanza a ritmo fijo (~16,7 ms). Recordamos cuando cambio por ultima
  // vez: durante los primeros 14 ms no puede volver a cambiar, asi que dormimos;
  // en el tramo final giramos con yield para enterarnos al instante (si dormimos
  // siempre, a veces llegabamos 1-2 ms tarde y se perdia el cuadro).
  static uint64_t s_last_value = 0;
  static std::chrono::steady_clock::time_point s_last_change{};
  const auto now = std::chrono::steady_clock::now();
  if (ctx.r3.u64 != s_last_value) {
    s_last_value = ctx.r3.u64;
    s_last_change = now;
  }
  if (ctx.r3.u64 < PPC_LOAD_U32(ctx.r30.u32 + 7404)) {
    if (now - s_last_change < std::chrono::milliseconds(14)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } else {
      std::this_thread::yield();
    }
  }
}

// ---------------------------------------------------------------------------
// Parche de CPU: los hilos trabajadores (sub_825FF208) buscan tareas con
//   while (...) { WaitForSingleObject(evento, 0); sub_8247F680(); }
// tiempo 0 = no esperan nada y giran (un nucleo cada uno). En esa llamada usamos
// 1 ms: si el evento se activa despiertan al instante; si no, duermen 1 ms.
// ---------------------------------------------------------------------------
extern "C" PPC_FUNC(__imp__sub_822CC440);  // WaitForSingleObject(r3 = handle, r4 = ms)

extern "C" PPC_FUNC(sub_822CC440) {
  if (REXCVAR_GET(cpu_patch_workers) && static_cast<uint32_t>(ctx.lr) == 0x825FF270 &&
      ctx.r4.u32 == 0) {
    ctx.r4.u64 = 1;
  }
  __imp__sub_822CC440(ctx, base);
}

// ---------------------------------------------------------------------------
// Parche de sincronizacion (notas vs musica): duracion REAL de cada cuadro.
// El modulo de tiempo del juego (bloque 0x8298BFA8) calcula la duracion de cada
// cuadro sumando los intervalos entre vblanks del cuadro (sub_8239FE30) y el dt
// es el promedio de los ultimos 4 cuadros (+96..+120 -> +56 y dt global
// 0x8285684C). En la consola los vblanks son perfectamente regulares; aqui llegan
// desparejos (3-30 ms) y esa suma perdia ~1,5% del tiempo: el carril de notas se
// atrasaba respecto de la musica (~2 s por cancion). Medido con mftb el 28/10/2026.
// Reemplazamos la duracion guardada por el tiempo real entre cuadros (mftb).
// ---------------------------------------------------------------------------
REXCVAR_DEFINE_BOOL(fix_frame_time, true, "ReWor",
                    "Duracion de cada cuadro medida en tiempo real (sincroniza notas y musica)");

static void StoreGuestDouble(uint8_t* base, uint32_t addr, double value) {
  uint64_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  PPC_STORE_U64(addr, bits);
}

static double SecondsSince(uint64_t& last_ticks) {
  const uint64_t now = rex::chrono::Clock::QueryGuestTickCount();
  const uint64_t prev = last_ticks;
  last_ticks = now;
  if (prev == 0 || now <= prev) return -1.0;
  return double(now - prev) / double(rex::chrono::Clock::guest_tick_frequency());
}

// ClockDiag (diagnostico opcional, --diag_clock_scan=true): cada 10 s busca en la memoria de
// datos del juego (0x82800000-0x82C90000) los valores que avanzan como relojes (segundos en
// float, milisegundos, cuadros o muestras en enteros) y anota su ritmo frente al tiempo real.
// Asi se encontro el reloj de la cancion adelantado (29/10/2026).
REXCVAR_DEFINE_BOOL(diag_clock_scan, false, "ReWor",
                    "Diagnostico: anotar en el log los relojes del juego y su ritmo cada 10 s");

static void ClockDiagThread(uint8_t* base) {
  constexpr uint32_t kStart = 0x82800000;
  constexpr uint32_t kSize = 0x490000;
  std::vector<uint8_t> prev(kSize), cur(kSize);
  std::memcpy(prev.data(), base + kStart, kSize);
  auto prev_time = std::chrono::steady_clock::now();
  for (uint32_t scan = 1;; ++scan) {
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::memcpy(cur.data(), base + kStart, kSize);
    const auto now = std::chrono::steady_clock::now();
    const double real = std::chrono::duration<double>(now - prev_time).count();
    uint32_t logged = 0;
    for (uint32_t off = 0; off + 8 <= kSize && logged < 60; off += 4) {
      uint32_t a, b;
      std::memcpy(&a, &prev[off], 4);
      std::memcpy(&b, &cur[off], 4);
      if (a == b) continue;
      a = _byteswap_ulong(a);
      b = _byteswap_ulong(b);
      float fa, fb;
      std::memcpy(&fa, &a, 4);
      std::memcpy(&fb, &b, 4);
      const double rf = (double(fb) - double(fa)) / real;
      if (std::isfinite(fa) && std::isfinite(fb) && fa > 1.0f && fb < 1.0e6f && rf > 0.8 &&
          rf < 1.25) {
        REXLOG_WARN("ClockDiag {}: f32 {:08X} {:.3f} -> {:.3f} ritmo={:.5f}", scan, kStart + off,
                    fa, fb, rf);
        ++logged;
      }
      if (b > a) {
        const double ru = double(b - a) / real;
        if ((ru > 50.0 && ru < 70.0) || (ru > 800.0 && ru < 1250.0) ||
            (ru > 40000.0 && ru < 50000.0)) {
          REXLOG_WARN("ClockDiag {}: u32 {:08X} {} -> {} ritmo={:.3f}", scan, kStart + off, a, b,
                      ru);
          ++logged;
        }
      }
      if ((off & 7) == 0) {
        uint64_t a8, b8;
        std::memcpy(&a8, &prev[off], 8);
        std::memcpy(&b8, &cur[off], 8);
        a8 = _byteswap_uint64(a8);
        b8 = _byteswap_uint64(b8);
        double da, db;
        std::memcpy(&da, &a8, 8);
        std::memcpy(&db, &b8, 8);
        const double rd = (db - da) / real;
        if (std::isfinite(da) && std::isfinite(db) && da > 1.0 && db < 1.0e7 && rd > 0.8 &&
            rd < 1.25) {
          REXLOG_WARN("ClockDiag {}: f64 {:08X} {:.4f} -> {:.4f} ritmo={:.5f}", scan, kStart + off,
                      da, db, rd);
          ++logged;
        }
      }
    }
    REXLOG_WARN("ClockDiag {}: fin del escaneo ({:.3f} s reales, {} relojes)", scan, real, logged);
    std::swap(prev, cur);
    prev_time = now;
  }
}

extern "C" PPC_FUNC(__imp__sub_823A0198);  // guarda la duracion del cuadro en +96[+44]
extern "C" PPC_FUNC(sub_823A0198) {
  static std::once_flag s_clock_diag_once;
  if (REXCVAR_GET(diag_clock_scan)) {
    std::call_once(s_clock_diag_once, [base] { std::thread(ClockDiagThread, base).detach(); });
  }
  constexpr uint32_t kTmr = 0x8298BFA8;
  const uint32_t slot = PPC_LOAD_U32(kTmr + 44);  // casilla (0..3) que se va a escribir
  __imp__sub_823A0198(ctx, base);
  static uint64_t s_last = 0;
  const double secs = SecondsSince(s_last);
  if (REXCVAR_GET(fix_frame_time) && secs >= 0.0 && slot < 4) {
    StoreGuestDouble(base, kTmr + 96 + slot * 8, secs);
  }
}

// ---------------------------------------------------------------------------
// Parche de sincronizacion 3: reloj de la cancion (0x82993A58, el que siguen las notas).
// sub_823D1490 (una vez por cuadro del juego, desde sub_821E51A0) le suma
// sub_8239FE30(n) = duracion de los ultimos n intervalos entre vblanks (n = vblanks
// desde la vuelta anterior, minimo 1). Esos intervalos los mide el manejador de vblank
// del juego, que aqui corre con retraso variable, y la suma salia entre +0,5% y +3%
// (medido 29/10/2026: 1,017 x tiempo real): las notas terminaban ~5-10 s antes que la
// musica. Cuando la llamada viene de ahi devolvemos el tiempo real entre llamadas; si
// pasaron mas de 0,1 s (cargas, reinicio de la cancion) se deja el valor del juego.
// ---------------------------------------------------------------------------
REXCVAR_DEFINE_BOOL(fix_song_time, true, "ReWor",
                    "Reloj de la cancion en tiempo real (evita que las notas se adelanten)");

extern "C" PPC_FUNC(__imp__sub_8239FE30);  // duracion (s) de los ultimos r3 intervalos de vblank
extern "C" PPC_FUNC(sub_8239FE30) {
  const uint32_t lr = static_cast<uint32_t>(ctx.lr);
  __imp__sub_8239FE30(ctx, base);
  if (lr != 0x823D1524) return;  // solo la llamada de sub_823D1490 (reloj de la cancion)
  static uint64_t s_last = 0;
  const double secs = SecondsSince(s_last);
  if (!REXCVAR_GET(fix_song_time) || secs < 0.0 || secs > 0.1) return;
  ctx.f1.f64 = secs;
}

// ---------------------------------------------------------------------------
// DLC: el juego no importa las funciones de XAM que usa para el contenido descargable; las
// pide por numero (ordinal) con XexGetProcedureAddress en sub_826C54B8 y las llama por
// puntero:
//   0x271 XamContentCreateInternal            (abrir un paquete, desde sub_826C56B8)
//   0x279 XamContentAggregateCreateEnumerator (buscar paquetes, desde sub_826C5800)
// El SDK v0.2.2 no puede devolver funciones asi ("no guest thunk available"): el juego no
// encontraba los DLC y despues los daba por danados. Reemplazo de sub_826C54B8 (r3 = numero,
// r4 = donde guardar la direccion; devuelve 0 si la encontro): para esos numeros entrega una
// direccion del juego que nunca es destino de llamadas (la segunda instruccion de la funcion
// que la pide) y apunta su casilla de la tabla de llamadas indirectas a la funcion del SDK.
// ---------------------------------------------------------------------------
extern "C" PPC_FUNC(__imp__XamContentCreateInternal);
extern "C" PPC_FUNC(__imp__XamContentAggregateCreateEnumerator);

extern "C" PPC_FUNC(__imp__sub_826C54B8);
extern "C" PPC_FUNC(sub_826C54B8) {
  struct XamOrdinalThunk {
    uint32_t ordinal;
    uint32_t guest_address;
    PPCFunc* function;
  };
  static const XamOrdinalThunk kThunks[] = {
      {0x271, 0x826C56BC, __imp__XamContentCreateInternal},
      {0x279, 0x826C5804, __imp__XamContentAggregateCreateEnumerator},
  };
  for (const XamOrdinalThunk& thunk : kThunks) {
    if (ctx.r3.u32 == thunk.ordinal) {
      PPC_LOOKUP_FUNC(base, thunk.guest_address) = thunk.function;
      PPC_STORE_U32(ctx.r4.u32, thunk.guest_address);
      ctx.r3.u64 = 0;  // ERROR_SUCCESS
      return;
    }
  }
  __imp__sub_826C54B8(ctx, base);
}

// ---------------------------------------------------------------------------
// Parche de sincronizacion 2: frecuencia del reloj del juego.
// sub_823A0238 (inicio del modulo Tmr) cambia la frecuencia de 50 000 000 a
// 49 875 000 Hz porque en la Xbox real el timebase va a 49,875 MHz aunque el sistema
// diga 50. En el SDK mftb va a 50 MHz exactos, asi que el reloj en milisegundos del
// juego (sub_8239FEF8, ~370 llamadas) corria un 0,25% rapido: las notas se iban
// adelantando a la musica (~0,5 s al final de una cancion; se corregia al pausar).
// Dejamos 50 MHz y recalculamos las dos constantes derivadas.
// ---------------------------------------------------------------------------
REXCVAR_DEFINE_BOOL(fix_timebase_freq, true, "ReWor",
                    "Reloj del juego a 50 MHz como el SDK (evita que las notas se adelanten)");

static double LoadGuestDouble(uint8_t* base, uint32_t addr) {
  const uint64_t bits = PPC_LOAD_U64(addr);
  double value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

extern "C" PPC_FUNC(__imp__sub_823A0238);  // inicio del modulo Tmr (frecuencia en +32)
extern "C" PPC_FUNC(sub_823A0238) {
  __imp__sub_823A0238(ctx, base);
  constexpr uint32_t kTmr = 0x8298BFA8;
  constexpr uint64_t kXboxTimebase = 49875000;
  constexpr uint64_t kSdkTimebase = 50000000;
  if (!REXCVAR_GET(fix_timebase_freq) || PPC_LOAD_U64(kTmr + 32) != kXboxTimebase) return;
  const double ratio = double(kSdkTimebase) / double(kXboxTimebase);
  PPC_STORE_U64(kTmr + 32, kSdkTimebase);
  // 0x8298C1D0 = ticks por microsegundo, 0x8298C0B0 = microsegundos por tick.
  StoreGuestDouble(base, 0x8298C1D0, LoadGuestDouble(base, 0x8298C1D0) * ratio);
  StoreGuestDouble(base, 0x8298C0B0, LoadGuestDouble(base, 0x8298C0B0) / ratio);
  REXLOG_INFO("ReWor: reloj del juego {} -> {} Hz (ticks/us={:.4f})", kXboxTimebase,
              kSdkTimebase, LoadGuestDouble(base, 0x8298C1D0));
}

// dt del hilo de dibujo (+52): lo lee una sola funcion (sub_82432EC0) una vez por
// vuelta de su bucle (~56,6 vueltas/s), pero el juego lo calcula con los cuadros
// dibujados (~57,9/s): sus relojes iban al 97,8%. Devolvemos el tiempo real entre
// llamadas (promedio de 4; cada muestra con tope de 0,1 s por pausas o cargas).
extern "C" PPC_FUNC(__imp__sub_8239FE20);  // devuelve en f1 el dt +52
extern "C" PPC_FUNC(sub_8239FE20) {
  __imp__sub_8239FE20(ctx, base);
  static uint64_t s_last = 0;
  static double s_hist[4] = {};
  static uint32_t s_count = 0;
  double secs = SecondsSince(s_last);
  if (secs < 0.0) return;
  if (secs > 0.1) secs = 0.1;
  s_hist[s_count++ & 3] = secs;
  if (!REXCVAR_GET(fix_frame_time) || s_count < 4) return;
  const double avg = (s_hist[0] + s_hist[1] + s_hist[2] + s_hist[3]) * 0.25;
  ctx.f1.f64 = double(static_cast<float>(avg));
}

// ---------------------------------------------------------------------------
// Safe stub for unmapped PPC functions — prevents RIP=0x0 crashes
// ---------------------------------------------------------------------------
#ifdef REWOR_DEV
static std::unordered_set<uint32_t> g_logged_unmapped;
#endif

static PPC_FUNC(unmapped_func_stub) {
  uint32_t target = ctx.ctr.u32;
  uint32_t caller = static_cast<uint32_t>(ctx.lr);
#ifdef REWOR_DEV
  if (g_logged_unmapped.find(target) == g_logged_unmapped.end()) {
    g_logged_unmapped.insert(target);
    TRACE("[UNMAPPED] PPC call to 0x" << std::hex << target 
          << " from 0x" << caller << " - stubbed (total unmapped: " 
          << std::dec << g_logged_unmapped.size() << ")");
  }
#endif
#ifdef REWOR_DEV
  StubTracker::instance().record_unmapped(target, caller);
#endif
  ctx.r3.u64 = 0;  // Safe default return value for unmapped functions
}

// Fill null entries in the dispatch table with our safe stub
static void fill_dispatch_table(uint8_t* base) {
  TRACE("[dispatch] Filling null entries in function dispatch table...");
  
  auto* table_base = reinterpret_cast<PPCFunc**>(
      base + PPC_IMAGE_BASE + PPC_IMAGE_SIZE);
  
  size_t num_entries = PPC_CODE_SIZE / 4;
  size_t filled = 0;
  
  for (size_t i = 0; i < num_entries; i++) {
    if (table_base[i] == nullptr) {
      table_base[i] = &unmapped_func_stub;
      filled++;
    }
  }
  
  TRACE("[dispatch] Filled " << std::dec << filled << " null entries out of " 
        << num_entries << " total (" << (num_entries - filled) << " mapped)");
}


class ReWorApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  ~ReWorApp() override { discord_shutdown(); }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    TRACE("[OnPreSetup] entered");
    config.audio_factory = REX_AUDIO_BACKEND(rex::audio::sdl::SDLAudioSystem);
    config.input_factory = REX_INPUT_BACKEND(rex::input::CreateDefaultInputSystem);
  }

  void OnPostSetup() override {
    TRACE("[OnPostSetup] entered - XEX loaded, runtime ready");
    
    uint8_t* base = runtime()->virtual_membase();
    fill_dispatch_table(base);

    discord_init();
    discord_update();

    // F11: Toggle borderless fullscreen
    rex::ui::RegisterBind("fullscreen_toggle", "F11", "Toggle fullscreen", [this]() {
        window()->SetFullscreen(!window()->IsFullscreen());
    });
#ifdef REWOR_DEV
    StubTracker::instance().start_reporter(5);
#endif
    TRACE("[OnPostSetup] StubTracker reporter started (5s interval -> re_wor_stub_freq.log)");
    
    TRACE("[OnPostSetup] sub_822CCB60 is hooked via PPC_FUNC - will be called by runtime during module init");
  }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* imgui) override {
#ifdef _WIN32
    // Set window icon from embedded resource
    auto* win32_window = dynamic_cast<rex::ui::Win32Window*>(window());
    if (win32_window) {
      HWND hwnd = win32_window->hwnd();
      if (hwnd) {
        HICON icon = LoadIconA(GetModuleHandleA(NULL), "IDI_ICON1");
        if (icon) {
          SendMessageA(hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
          SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
        }
      }
    }
#endif
    // Start in borderless fullscreen by default
    window()->SetFullscreen(true);
  }

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
#ifdef _WIN32
    SetUnhandledExceptionFilter(CrashHandler);
#endif
TRACE("[Create] constructing ReWorApp");
    auto app = std::unique_ptr<ReWorApp>(new ReWorApp(ctx, "Guitar Hero Warriors of Rock",
        {PPC_CODE_BASE, PPC_CODE_SIZE, PPC_IMAGE_BASE,
         PPC_IMAGE_SIZE, PPCFuncMappings}));
    TRACE("[Create] done");
    return app;
  }
};

REX_DEFINE_APP(re_wor, ReWorApp::Create)
