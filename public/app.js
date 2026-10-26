// =============================================================================
//  app.js — Lógica del FRONTEND
//  -----------------------------------------------------------------------------
//  El navegador NO puede leer la base de datos directamente. En su lugar hace
//  peticiones HTTP al servidor C++ con fetch(), y el servidor responde JSON.
//
//      Navegador (app.js)  --fetch-->  Servidor C++  --SQL-->  SQLite
//                          <--JSON--                 <--filas--
// =============================================================================

const API = '/api';

// Referencias a elementos del HTML
const $ = (id) => document.getElementById(id);
const formulario  = $('formulario');
const tabla       = $('tabla');
const buscar      = $('buscar');
const btnCancelar = $('btn-cancelar');

// -----------------------------------------------------------------------------
//  Función genérica para llamar a la API.
//  Si el servidor responde con error (4xx/5xx), lanza una excepción con el
//  mensaje que el servidor envió en {"error": "..."}.
// -----------------------------------------------------------------------------
async function api(ruta, opciones = {}) {
  const resp = await fetch(API + ruta, {
    headers: { 'Content-Type': 'application/json' },
    ...opciones,
  });
  const datos = await resp.json();
  if (!resp.ok) throw new Error(datos.error || `Error ${resp.status}`);
  return datos;
}

// -----------------------------------------------------------------------------
//  Cargar y dibujar la tabla  (GET /api/estudiantes?q=...)
// -----------------------------------------------------------------------------
async function cargarEstudiantes() {
  const q = buscar.value.trim();
  const estudiantes = await api('/estudiantes' + (q ? '?q=' + encodeURIComponent(q) : ''));

  tabla.innerHTML = '';
  for (const e of estudiantes) {
    const fila = document.createElement('tr');
    // textContent (en lugar de innerHTML) evita que un nombre con etiquetas
    // HTML se ejecute en la página (ataque XSS).
    for (const valor of [e.id, e.nombre, e.apellido, e.seccion]) {
      const td = document.createElement('td');
      td.textContent = valor;
      fila.appendChild(td);
    }
    const tdNota = document.createElement('td');
    tdNota.className = 'num ' + (e.nota >= 70 ? 'ok' : 'mal');
    tdNota.textContent = e.nota.toFixed(1);
    fila.appendChild(tdNota);

    const tdAcc = document.createElement('td');
    tdAcc.innerHTML = '<div class="acciones">' +
      '<button class="chico" data-accion="editar">Editar</button>' +
      '<button class="chico peligro" data-accion="eliminar">Eliminar</button></div>';
    tdAcc.querySelector('[data-accion="editar"]').onclick   = () => editar(e);
    tdAcc.querySelector('[data-accion="eliminar"]').onclick = () => eliminar(e);
    fila.appendChild(tdAcc);

    tabla.appendChild(fila);
  }
  $('vacio').hidden = estudiantes.length > 0;
}

// -----------------------------------------------------------------------------
//  Estadísticas  (GET /api/estadisticas)
// -----------------------------------------------------------------------------
async function cargarEstadisticas() {
  const s = await api('/estadisticas');
  $('st-total').textContent      = s.total;
  $('st-promedio').textContent   = s.total ? s.promedio.toFixed(1) : '–';
  $('st-aprobados').textContent  = s.aprobados;
  $('st-reprobados').textContent = s.reprobados;
}

async function refrescar() {
  try {
    await Promise.all([cargarEstudiantes(), cargarEstadisticas()]);
  } catch (err) {
    avisar('No se pudo conectar con el servidor: ' + err.message, true);
  }
}

// -----------------------------------------------------------------------------
//  Guardar: crea (POST) o actualiza (PUT) según haya id o no
// -----------------------------------------------------------------------------
formulario.addEventListener('submit', async (ev) => {
  ev.preventDefault();   // evita que el navegador recargue la página

  const id = $('id').value;
  const datos = {
    nombre:   $('nombre').value.trim(),
    apellido: $('apellido').value.trim(),
    seccion:  $('seccion').value.trim(),
    nota:     parseFloat($('nota').value),
  };

  try {
    if (id) {
      await api('/estudiantes/' + id, { method: 'PUT', body: JSON.stringify(datos) });
      avisar('Estudiante actualizado');
    } else {
      await api('/estudiantes', { method: 'POST', body: JSON.stringify(datos) });
      avisar('Estudiante creado');
    }
    limpiarFormulario();
    refrescar();
  } catch (err) {
    avisar(err.message, true);
  }
});

// Pasa los datos de una fila al formulario para editarlos
function editar(e) {
  $('id').value       = e.id;
  $('nombre').value   = e.nombre;
  $('apellido').value = e.apellido;
  $('seccion').value  = e.seccion;
  $('nota').value     = e.nota;
  $('titulo-form').textContent = `Editar estudiante #${e.id}`;
  btnCancelar.hidden = false;
  $('nombre').focus();
}

// DELETE /api/estudiantes/{id}
async function eliminar(e) {
  if (!confirm(`¿Eliminar a ${e.nombre} ${e.apellido}?`)) return;
  try {
    await api('/estudiantes/' + e.id, { method: 'DELETE' });
    avisar('Estudiante eliminado');
    if ($('id').value == e.id) limpiarFormulario();
    refrescar();
  } catch (err) {
    avisar(err.message, true);
  }
}

function limpiarFormulario() {
  formulario.reset();
  $('id').value = '';
  $('titulo-form').textContent = 'Nuevo estudiante';
  btnCancelar.hidden = true;
}
btnCancelar.addEventListener('click', limpiarFormulario);

// -----------------------------------------------------------------------------
//  Búsqueda: espera 250 ms después de la última tecla antes de consultar
//  (técnica "debounce", evita una petición por cada letra).
// -----------------------------------------------------------------------------
let temporizador;
buscar.addEventListener('input', () => {
  clearTimeout(temporizador);
  temporizador = setTimeout(cargarEstudiantes, 250);
});

// Mensaje emergente que desaparece solo
let temporizadorAviso;
function avisar(texto, esError = false) {
  const el = $('aviso');
  el.textContent = texto;
  el.className = 'aviso' + (esError ? ' error' : '');
  el.hidden = false;
  clearTimeout(temporizadorAviso);
  temporizadorAviso = setTimeout(() => (el.hidden = true), 3000);
}

// Al cargar la página
refrescar();
