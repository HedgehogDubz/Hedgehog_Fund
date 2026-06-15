import React from 'react';

const s = { width: 16, height: 16, display: 'block' };

// icons8-home.svg (inline SVG)
export function HomeIcon({ color = 'currentColor' }) {
  return (
    <svg viewBox="0 0 24 24" style={s} fill={color}>
      <path d="M12 2a1 1 0 00-.711.297L1.203 11.098A.5.5 0 001 11.5a.5.5 0 00.5.5H4v8a1 1 0 001 1h4a1 1 0 001-1v-6h4v6a1 1 0 001 1h4a1 1 0 001-1v-8h2.5a.5.5 0 00.5-.5.5.5 0 00-.203-.402L12.717 2.303A1 1 0 0012 2z"/>
    </svg>
  );
}

// icons8-hand-30.png (data URI)
const HAND_PNG = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAB4AAAAeCAYAAAA7MK6iAAAACXBIWXMAAAsTAAALEwEAmpwYAAAA0ElEQVR4nO3UPWoCQRgG4KewSae9dbCwEM+Qg3iVFBEvkMIjeIIUIZeQQMgJ0iQQUUTTjAgbkEVYdmd3J4R9YYphGB6+b37oUpw5dnjQco4IOLRdYbgY19YbqzDk4MY6EHJQ0TwZPK+r9aEkXFvrQ0m4ttaHDpYADiVHB/8NeIJXbPGCu7bgtysbnjBuGp5lf2t+0w8WTZ/xCOtIpPLlusEyBfybx1RwD88p4HMGeI9Ev1XMLb4i4JWITPFRAf3EUGT6uM+e274A3GSVRqP+XU5XVGLOyJjRHwAAAABJRU5ErkJggg==';

export function HandIcon({ color }) {
  return <img src={HAND_PNG} alt="Pan" style={{ ...s, filter: color === '#000' ? 'brightness(0)' : 'brightness(0) invert(1)' }} />;
}

// icons8-magnifying-glass.svg (inline SVG)
export function ZoomIcon({ color = 'currentColor' }) {
  return (
    <svg viewBox="0 0 64 64" style={s} fill={color}>
      <path d="M24 2.889C12.366 2.889 2.889 12.366 2.889 24S12.366 45.111 24 45.111c5.037 0 9.665-1.78 13.299-4.738l14.832 18.58s3.249.483 5.266-1.62c2.03-2.117 1.555-5.198 1.555-5.198L40.373 37.3C43.331 33.665 45.111 29.037 45.111 24 45.111 12.366 35.634 2.889 24 2.889zm0 4.222c9.353 0 16.889 7.536 16.889 16.889S33.353 40.889 24 40.889 7.111 33.353 7.111 24 14.647 7.111 24 7.111z"/>
    </svg>
  );
}

// icons8-resize-horizontal-50.png (data URI)
const HRESIZE_PNG = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADIAAAAyCAYAAAAeP4ixAAAACXBIWXMAAAsTAAALEwEAmpwYAAABLklEQVR4nO3XP0vDQBjH8a/gS9G+CKXVJU46OEmHTuog9b0VHFwURH0LunTqOxDqPxxMpZCTEHItJveQO/l94cbckw/hQgJKKaWUUkoFbgRMgUegF3pzYAt4AJ6AIUZdADmwKNa1wYxJaf+8mGmKWK770EOAm8qMoJg6xCewZ/h2gHcLzPnwXdn4CzjErj7wWoO5TAnhGgBvITBdIlwZ8NEGEwOiNSYmRGNMjAjXQfGmXIupQ6Sw8jImVcSiijn7B5CxeyqpYvIyYhVmediPiPewj30XxIjJPK9fLyJGTNYUERMma4uIATPwfDT+GdElpu/5jG+MWPdjtUv49q1+rFZhbgnf1BLhw1yFHgDMrBGuE+AZuAO2DfY/Bl6AOXBK4m0Am13fhFJKKaWU4rcfk1KKDLHm/FAAAAAASUVORK5CYII=';

export function HResizeIcon({ color }) {
  return <img src={HRESIZE_PNG} alt="Resize Horizontal" style={{ ...s, filter: color === '#000' ? 'brightness(0)' : 'brightness(0) invert(1)' }} />;
}

// icons8-resize-vertical-50.png (data URI)
const VRESIZE_PNG = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADIAAAAyCAYAAAAeP4ixAAAACXBIWXMAAAsTAAALEwEAmpwYAAABWElEQVR4nO3aO0pEMRiG4XclgroAsbByBNFtaGNplfECXrZjYSGDloJWrkFr1+Cl9chAhCH8OcmgE78c8kKanEzIw3CmCAOLz/lRdQ7o/DhjAIiuVowzENVhXA+iGozLQMhj3BwIWYwzDvmZOefUEbvG/BbwrvjNuB4ExjMUMS6BiEGkMPsRxE6wLgbBr7Xemb2CDl4yEClIDPNMwR4zEDkQC/NAwVaAa+AW2OhZlwPB73Hn91xGsFyIfF2DiNUgajWIWg2iVoOo1SBqNYhaDaJWg6glD1kFboAJsP4HkDW/1xWwRMGeZg43vTEc/QIyCm4d7ynYa3DAGCYFCRGdv/wr1qFxSAvTB7EQHXBA4Y6NQ3wA2xmQTeDNeH7JP5XCWBA5xE9HPZhwPoa4QKTTyDuTMzf9rFRj45CpcY5o4yEg5sHIIwb1F45ZzFftCOvXrFoEvhM/Fto36/CJ2bkvgzQAAAAASUVORK5CYII=';

export function VResizeIcon({ color }) {
  return <img src={VRESIZE_PNG} alt="Resize Vertical" style={{ ...s, filter: color === '#000' ? 'brightness(0)' : 'brightness(0) invert(1)' }} />;
}
