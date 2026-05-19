import { jsx as _jsx } from "react/jsx-runtime";
import { describe, it, expect, vi, beforeEach, afterEach } from "vitest";
import { render, screen, act } from "@testing-library/react";
// Mock the image import — jsdom can't load binary assets
vi.mock("../assets/splash-logo.png", () => ({ default: "splash-logo.png" }));
import { SplashScreen } from "./SplashScreen";
beforeEach(() => { vi.useFakeTimers(); });
afterEach(() => { vi.useRealTimers(); });
describe("SplashScreen – visibility", () => {
    it("renders the logo image when visible", () => {
        render(_jsx(SplashScreen, { visible: true }));
        const img = screen.getByAltText("CluicheEditor");
        expect(img).toBeInTheDocument();
        expect(img).toHaveAttribute("src", "splash-logo.png");
    });
    it("renders the spinner element when visible", () => {
        const { container } = render(_jsx(SplashScreen, { visible: true }));
        // The spinner is a div with a circular border-radius style
        const spinner = container.querySelector('div[style*="border-radius: 50%"]');
        expect(spinner).toBeInTheDocument();
    });
    it("has opacity 1 when visible is true", () => {
        const { container } = render(_jsx(SplashScreen, { visible: true }));
        const wrapper = container.firstElementChild;
        expect(wrapper).toHaveStyle({ opacity: "1" });
    });
    it("has opacity 0 when visible becomes false", () => {
        const { container, rerender } = render(_jsx(SplashScreen, { visible: true }));
        rerender(_jsx(SplashScreen, { visible: false }));
        const wrapper = container.firstElementChild;
        expect(wrapper).toHaveStyle({ opacity: "0" });
    });
    it("still mounted immediately after visible becomes false (fade-out in progress)", () => {
        const { container, rerender } = render(_jsx(SplashScreen, { visible: true }));
        rerender(_jsx(SplashScreen, { visible: false }));
        expect(container.firstElementChild).not.toBeNull();
    });
    it("unmounts after the 400ms fade-out delay", () => {
        const { container, rerender } = render(_jsx(SplashScreen, { visible: true }));
        rerender(_jsx(SplashScreen, { visible: false }));
        act(() => { vi.advanceTimersByTime(400); });
        expect(container.firstElementChild).toBeNull();
    });
    it("does NOT unmount before 400ms have elapsed", () => {
        const { container, rerender } = render(_jsx(SplashScreen, { visible: true }));
        rerender(_jsx(SplashScreen, { visible: false }));
        act(() => { vi.advanceTimersByTime(399); });
        expect(container.firstElementChild).not.toBeNull();
    });
});
describe("SplashScreen – rapid toggle", () => {
    it("re-showing before 400ms cancels the unmount timeout", () => {
        const { container, rerender } = render(_jsx(SplashScreen, { visible: true }));
        // Hide it — starts the 400ms countdown
        rerender(_jsx(SplashScreen, { visible: false }));
        act(() => { vi.advanceTimersByTime(200); });
        // Show it again before the timeout fires
        rerender(_jsx(SplashScreen, { visible: true }));
        act(() => { vi.advanceTimersByTime(400); });
        // Should still be mounted because visible=true (no pending unmount)
        expect(container.firstElementChild).not.toBeNull();
    });
    it("hiding again after re-show restarts the 400ms timer", () => {
        const { container, rerender } = render(_jsx(SplashScreen, { visible: true }));
        rerender(_jsx(SplashScreen, { visible: false }));
        act(() => { vi.advanceTimersByTime(200); });
        rerender(_jsx(SplashScreen, { visible: true }));
        // Hide it a second time — fresh 400ms countdown
        rerender(_jsx(SplashScreen, { visible: false }));
        act(() => { vi.advanceTimersByTime(399); });
        expect(container.firstElementChild).not.toBeNull();
        act(() => { vi.advanceTimersByTime(1); });
        expect(container.firstElementChild).toBeNull();
    });
});
