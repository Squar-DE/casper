use glib::subclass::prelude::*;
use std::path::Path;
use glib::ObjectExt;
use glib::subclass::prelude::*;
use glib::{ParamSpec, Properties, Value};
use std::cell::RefCell;

mod imp {
    use super::*;

    #[derive(Default, Properties)]
    #[properties(wrapper_type = super::FileItem)]
    pub struct FileItem {
        #[property(get, set)]
        pub name: RefCell<String>,
        #[property(get, set)]
        pub path: RefCell<String>,
        #[property(get, set)]
        pub icon: RefCell<String>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for FileItem {
        const NAME: &'static str = "FileItem";
        type Type = super::FileItem;
    }

    impl ObjectImpl for FileItem {
        fn properties() -> &'static [ParamSpec] {
            Self::derived_properties()
        }

        fn set_property(&self, id: usize, value: &Value, pspec: &ParamSpec) {
            self.derived_set_property(id, value, pspec)
        }

        fn property(&self, id: usize, pspec: &ParamSpec) -> Value {
            self.derived_property(id, pspec)
        }
    }
}

glib::wrapper! {
    pub struct FileItem(ObjectSubclass<imp::FileItem>);
}

impl FileItem {
    pub fn new(name: &str, path: &str, icon: &str) -> Self {
        glib::Object::builder()
            .property("name", name)
            .property("path", path)
            .property("icon", icon)
            .build()
    }
}

// File type icon mappings
pub struct FileTypeIcon {
    pub extension: &'static str,
    pub icon_name: &'static str,
}

// List of file type icons
pub const FILE_TYPE_ICONS: &[FileTypeIcon] = &[
    // Documents
    FileTypeIcon { extension: "txt", icon_name: "text-x-generic" },
    FileTypeIcon { extension: "pdf", icon_name: "application-pdf" },
    FileTypeIcon { extension: "doc", icon_name: "x-office-document" },
    FileTypeIcon { extension: "docx", icon_name: "x-office-document" },
    FileTypeIcon { extension: "odt", icon_name: "x-office-document" },
    
    // Spreadsheets
    FileTypeIcon { extension: "xls", icon_name: "x-office-spreadsheet" },
    FileTypeIcon { extension: "xlsx", icon_name: "x-office-spreadsheet" },
    FileTypeIcon { extension: "ods", icon_name: "x-office-spreadsheet" },
    
    // Presentations
    FileTypeIcon { extension: "ppt", icon_name: "x-office-presentation" },
    FileTypeIcon { extension: "pptx", icon_name: "x-office-presentation" },
    FileTypeIcon { extension: "odp", icon_name: "x-office-presentation" },
    
    // Images
    FileTypeIcon { extension: "jpg", icon_name: "image-x-generic" },
    FileTypeIcon { extension: "jpeg", icon_name: "image-x-generic" },
    FileTypeIcon { extension: "png", icon_name: "image-x-generic" },
    FileTypeIcon { extension: "gif", icon_name: "image-x-generic" },
    FileTypeIcon { extension: "svg", icon_name: "image-x-generic" },
    FileTypeIcon { extension: "webp", icon_name: "image-x-generic" },
    
    // Audio
    FileTypeIcon { extension: "mp3", icon_name: "audio-x-generic" },
    FileTypeIcon { extension: "ogg", icon_name: "audio-x-generic" },
    FileTypeIcon { extension: "wav", icon_name: "audio-x-generic" },
    FileTypeIcon { extension: "flac", icon_name: "audio-x-generic" },
    
    // Video
    FileTypeIcon { extension: "mp4", icon_name: "video-x-generic" },
    FileTypeIcon { extension: "webm", icon_name: "video-x-generic" },
    FileTypeIcon { extension: "mkv", icon_name: "video-x-generic" },
    FileTypeIcon { extension: "avi", icon_name: "video-x-generic" },
    
    // Archives
    FileTypeIcon { extension: "zip", icon_name: "package-x-generic" },
    FileTypeIcon { extension: "tar", icon_name: "package-x-generic" },
    FileTypeIcon { extension: "rar", icon_name: "package-x-generic" },
    FileTypeIcon { extension: "gz", icon_name: "package-x-generic" },
    FileTypeIcon { extension: "7z", icon_name: "package-x-generic" },
    
    // Code
    FileTypeIcon { extension: "c", icon_name: "text-x-script" },
    FileTypeIcon { extension: "cpp", icon_name: "text-x-script" },
    FileTypeIcon { extension: "h", icon_name: "text-x-script" },
    FileTypeIcon { extension: "rs", icon_name: "text-x-script" },
    FileTypeIcon { extension: "py", icon_name: "text-x-script" },
    FileTypeIcon { extension: "js", icon_name: "text-x-script" },
    FileTypeIcon { extension: "html", icon_name: "text-html" },
    FileTypeIcon { extension: "css", icon_name: "text-x-script" },
    FileTypeIcon { extension: "json", icon_name: "text-x-script" },
    FileTypeIcon { extension: "xml", icon_name: "text-xml" },
    
    // Executables
    FileTypeIcon { extension: "exe", icon_name: "application-x-executable" },
    FileTypeIcon { extension: "sh", icon_name: "application-x-executable" },
    FileTypeIcon { extension: "appimage", icon_name: "application-x-executable" },
];

// Get icon name for file based on extension
pub fn get_icon_for_file(file_path: &Path) -> &'static str {
    if file_path.is_dir() {
        return "folder";
    }
    
    if let Some(extension) = file_path.extension() {
        if let Some(ext_str) = extension.to_str() {
            for file_type in FILE_TYPE_ICONS {
                if ext_str.eq_ignore_ascii_case(file_type.extension) {
                    return file_type.icon_name;
                }
            }
        }
    }
    
    // Default icon for unknown file types
    "text-x-generic"
}
