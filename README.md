# Sisop-4-2025-IT06

Repository ini berisi hasil pengerjaan Praktikum Sistem Operasi 2025 Modul 4

| Nama                     | Nrp        |
| ------------------------ | ---------- |
| Paundra Pujo Darmawan    | 5027241008 |
| Putri Joselina Silitonga | 5027241116 |

# Soal 3 (Paundra Pujo Darmawan)

Pada soal ini, objective kita adalah menggunakan linux secara isolasi menggunakan docker. Jadi kita menggunakan image linux yang sudah ada dan menjalankan program didalam kontainer yang terisolasi. Pertama-tama kita dapat setting untuk membuat kontainer dengan [docker-compose.yml](soal_3/docker-compose.yml). Dan menjalankan program yang sudah kita buat kedalam kontainer tersebut dengan [Dockerfile](soal_3/Dockerfile).

Sedangkan pada program sendiri, kita membuat program yang menggunakan konsep FUSE secara sederhana, dengan objektif untuk mendeteksi program yang terdapat kata `nafis` ataupun `kimcun` di dalamnya. Dimana kita harus mereverse nama file tersebut. Dan untuk file yang tidak terdapat kata kata tersebut, kita harus melakukan encrypt menggunakan rot13 untuk melindungi isi file dari nafis dan kimcun.

Melakukan scanning untuk file yang memiliki kata kata `nafis` ataupun `kimcun`.

```c
int is_malicious(const char *filename) {
    return strstr(filename, "nafis") || strstr(filename, "kimcun");
}
```

Melakukan reverse string pada file yang mengandung kata kata `nafis` ataupun `kimcun` didalamnya.

```c
void reverse_string(char *content) {
    int l = 0;
    int r = strlen(content) - 1;
    char t;

    while (l < r) {
        t = content[l];
        content[l] = content[r];
        content[r] = t;

        l++;
        r--;
    }
}
```

Melakukan enkripsi pada file normal.

```c
char *rot13(const char *src)
{
    if(src == NULL){
      return NULL;
    }

    char* result = malloc(strlen(src)+1);

    if(result != NULL){
      strcpy(result, src);
      char* current_char = result;

      while(*current_char != '\0'){
        if((*current_char >= 97 && *current_char <= 122) || (*current_char >= 65 && *current_char <= 90)){
          if(*current_char > 109 || (*current_char > 77 && *current_char < 91)){
            *current_char -= 13;
          }else{
            *current_char += 13;
          }
        }
        current_char++;
      }
    }
    return result;
}
```
