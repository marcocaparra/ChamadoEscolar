module.exports = {
    uiHost: process.env.CHAMADO_ESCOLAR_HOST || "127.0.0.1",
    contextStorage: {
        default: "memoria",
        memoria: { module: "memory" },
        arquivo: {
            module: "localfilesystem",
            config: { cache: true, flushInterval: 5 }
        }
    }
};
