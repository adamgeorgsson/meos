/// <reference types="vite/client" />

interface ImportMetaEnv {
  readonly VITE_USE_BACKEND?: string;
}

interface ImportMeta {
  readonly env: ImportMetaEnv;
}
