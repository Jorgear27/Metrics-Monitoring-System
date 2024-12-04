/**
 * @file main.c
 * @brief Entry point of the system
 */

#include "expose_metrics.h"
#include <cjson/cJSON.h>
#include <fcntl.h>
#include <sys/stat.h>

/**
 * @brief Ruta del pipe para recibir configuraciones.
 */
#define PIPE_PATH "/tmp/monitor_pipe"

/**
 * @brief Configuración global.
 */
int sleep_time = 1;                    /**< Tiempo de espera entre actualizaciones de métricas. */
bool monitor_cpu_usage = true;         /**< Indica si se debe monitorear el uso de CPU. */
bool monitor_memory_usage = true;      /**< Indica si se debe monitorear el uso de memoria. */
bool monitor_disk = true;              /**< Indica si se debe monitorear el uso de disco. */
bool monitor_network = true;           /**< Indica si se debe monitorear el uso de red. */
bool monitor_processes_running = true; /**< Indica si se debe monitorear la cantidad de procesos en ejecución. */
bool monitor_context_switches = true;  /**< Indica si se debe monitorear la cantidad de cambios de contexto. */
bool monitor_fragmentation = true;     /**< Indica si se debe monitorear la fragmentación de memoria. */
bool monitor_policies = true;          /**< Indica si se debe monitorear las políticas de asignación de memoria. */

/**
 * @brief Punto de entrada del programa.
 *
 * @param argc Cantidad de argumentos.
 * @param argv Argumentos del programa.
 * @return int Código de salida.
 */
int main(int argc, char* argv[])
{
    // Avoid unused variable warning
    (void)argc;
    (void)argv;

    // Create the named pipe
    mkfifo(PIPE_PATH, 0666);

    int pipe_fd = open(PIPE_PATH, O_RDONLY | O_NONBLOCK);
    if (pipe_fd == -1)
    {
        perror("Failed to open pipe");
        return EXIT_FAILURE;
    }

    usleep(1000000); // 1 s, wait for the Shell to write the configuration

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(pipe_fd, buffer, sizeof(buffer) - 1)) == -1 && errno == EAGAIN)
    {
        // Wait until data is available
        usleep(100000); // 100 ms
    }

    if (bytes_read == -1)
    {
        perror("Failed to read from pipe");
        close(pipe_fd);
        return EXIT_FAILURE;
    }
    buffer[bytes_read] = '\0';
    close(pipe_fd);

    cJSON* json = cJSON_Parse(buffer);
    if (!json)
    {
        fprintf(stderr, "Error parsing config JSON: Default configuration will be used%s\n", cJSON_GetErrorPtr());
    }
    else
    {
        cJSON* sleep_time_item = cJSON_GetObjectItem(json, "sleep_time");
        if (cJSON_IsNumber(sleep_time_item))
        {
            sleep_time = sleep_time_item->valueint;
        }

        cJSON* metrics = cJSON_GetObjectItem(json, "metrics");
        if (metrics)
        {
            monitor_cpu_usage = cJSON_IsTrue(cJSON_GetObjectItem(metrics, "cpu_usage"));
            monitor_memory_usage = cJSON_IsTrue(cJSON_GetObjectItem(metrics, "memory_usage"));
            monitor_disk = cJSON_IsTrue(cJSON_GetObjectItem(metrics, "disk_usage"));
            monitor_network = cJSON_IsTrue(cJSON_GetObjectItem(metrics, "network_usage"));
            monitor_processes_running = cJSON_IsTrue(cJSON_GetObjectItem(metrics, "processes_running"));
            monitor_context_switches = cJSON_IsTrue(cJSON_GetObjectItem(metrics, "context_switches"));
            monitor_fragmentation = cJSON_IsTrue(cJSON_GetObjectItem(metrics, "fragmentation"));
            monitor_policies = cJSON_IsTrue(cJSON_GetObjectItem(metrics, "policies"));
        }
    }

    cJSON_Delete(json);

    // Create a thread to expose metrics via HTTP
    pthread_t metrics_tid;
    if (pthread_create(&metrics_tid, NULL, expose_metrics, NULL) != 0)
    {
        fprintf(stderr, "Error creating HTTP server thread\n");
        return EXIT_FAILURE;
    }

    printf("Configuration loaded\n");
    // Initialize mutex and metrics
    init_metrics(monitor_cpu_usage, monitor_memory_usage, monitor_disk, monitor_network, monitor_processes_running,
                 monitor_context_switches, monitor_fragmentation, monitor_policies);
    printf("Metrics initialized\n");
    // Main loop to update metrics
    while (true)
    {
        printf("Updating metrics\n");
        if (monitor_cpu_usage)
            update_cpu_gauge();
        if (monitor_memory_usage)
            update_memory_gauge();
        if (monitor_disk)
            update_disk_gauge();
        if (monitor_network)
            update_network_gauge();
        if (monitor_processes_running)
            update_processes_gauge();
        if (monitor_context_switches)
            update_context_switches_gauge();
        if (monitor_fragmentation)
            update_fragmentation_gauge();
        if (monitor_policies)
            update_policies_gauge();
        sleep(sleep_time);
    }

    return EXIT_SUCCESS;
}
