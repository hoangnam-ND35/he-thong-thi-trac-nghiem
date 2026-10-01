class ApiError extends Error {
  constructor(message, status) {
    super(message);
    this.status = status;
    this.offline = status === 0;
  }
}

const API = {
  simulateOffline: false,
  setOfflineSimulator(on) {
    this.simulateOffline = !!on;
  },
  async request(method, url, body) {
    if (this.simulateOffline) throw new ApiError("Mất kết nối", 0);
    let response;
    try {
      response = await fetch(url, {
        method,
        credentials: "same-origin",
        headers: {
          "Content-Type": "application/json",
          "X-Requested-With": "OnlineExam"
        },
        body: body === undefined ? undefined : JSON.stringify(body)
      });
    } catch (error) {
      throw new ApiError("Mất kết nối tới máy chủ", 0);
    }
    const data = await response.json().catch(() => ({}));
    const publicPage = /\/(login|register)\.html$/.test(location.pathname);
    if (response.status === 401 && !publicPage) {
      location.href = "/login.html";
    }
    if (!response.ok || data.ok === false) {
      throw new ApiError(data.error || "Lỗi máy chủ", response.status);
    }
    return data.data;
  },
  get(url) { return this.request("GET", url); },
  post(url, body) { return this.request("POST", url, body ?? {}); },
  put(url, body) { return this.request("PUT", url, body ?? {}); }
};
