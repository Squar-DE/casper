#include "filemanager.h"

// Format file size for display
char* format_file_size(goffset size) {
    if (size < 1024) {
        return g_strdup_printf("%ld B", size);
    } else if (size < 1024 * 1024) {
        return g_strdup_printf("%.1f KB", size / 1024.0);
    } else if (size < 1024 * 1024 * 1024) {
        return g_strdup_printf("%.1f MB", size / (1024.0 * 1024.0));
    } else {
        return g_strdup_printf("%.1f GB", size / (1024.0 * 1024.0 * 1024.0));
    }
}

// Format file modification time
char* format_file_time(GTimeVal *time) {
    GDateTime *dt = g_date_time_new_from_timeval_local(time);
    char *formatted = g_date_time_format(dt, "%Y-%m-%d %H:%M");
    g_date_time_unref(dt);
    return formatted;
}

// Get file icon name based on file type
const char* get_file_icon(GFile *file, GFileInfo *info) {
    GFileType type = g_file_info_get_file_type(info);
    const char *content_type = g_file_info_get_content_type(info);
    
    if (type == G_FILE_TYPE_DIRECTORY) {
        return "folder-symbolic";
    }
    
    if (content_type) {
        if (g_str_has_prefix(content_type, "image/")) {
            return "image-x-generic-symbolic";
        } else if (g_str_has_prefix(content_type, "text/")) {
            return "text-x-generic-symbolic";
        } else if (g_str_has_prefix(content_type, "audio/")) {
            return "audio-x-generic-symbolic";
        } else if (g_str_has_prefix(content_type, "video/")) {
            return "video-x-generic-symbolic";
        }
    }
    
    return "text-x-generic-symbolic";
}
