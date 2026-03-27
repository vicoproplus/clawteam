import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import { Provider } from "react-redux";
import { BrowserRouter, Route, Routes } from "react-router";
import App from "./App";
import { store } from "./store";
import "./index.css";

const container = document.getElementById("root")!;
const root = createRoot(container);

root.render(
  <StrictMode>
    <Provider store={store}>
      <BrowserRouter>
        <Routes>
          <Route index element={<App />} />
          <Route path="/session/:sessionId" element={<App />} />
        </Routes>
      </BrowserRouter>
    </Provider>
  </StrictMode>
);
