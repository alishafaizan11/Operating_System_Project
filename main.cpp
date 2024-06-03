#include <QCoreApplication>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <map>
#include <QTextStream>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <memory>

using namespace std;

namespace fs = filesystem;
// Function to execute a shell command and capture its output
string executeCommand(const char* cmd) {
   array<char, 128> buffer;
   string result;
   shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
   if (!pipe) {
       throw runtime_error("popen() failed!");
   }
   while (!feof(pipe.get())) {
       if (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
           result += buffer.data();
       }
   }
   return result;
}

class FileManager {
public:
  FileManager(const string& username) {
   currentPath = fs::current_path();
   cout << "Enter password for encryption/decryption: ";
   cin>>password;
   //generateKey();
   //getline(cin, password);
   //generateKey();
   //cin.ignore();
   usersKeys[username] = key;
  }

  void listFiles() {
   for (const auto& entry : fs::directory_iterator(currentPath)) {
       cout << entry.path().filename() << endl;
   }
  }

  void createDirectory(const string& dirName) {
   fs::create_directory(currentPath / dirName);
   cout << "Directory created: " << currentPath / dirName << endl;
  }

  void createFile(const string& fileName){
   ofstream outputFile(currentPath / fileName);

   if(outputFile.is_open()){
       outputFile << "Hello" <<endl;
       outputFile.close();
       cout << "File created successfully: " <<currentPath/fileName<<endl;
   }
   else
       cout << "Error creating the file. " <<endl;
  }

  void moveFile(const string& source, const string& destination) {
      fs::path sourcePath = currentPath / source;
      fs::path destinationPath = currentPath / destination;

      if (fs::exists(sourcePath)) {
          try {
              // Use fs::rename to move if source and destination are on the same filesystem
              fs::rename(sourcePath, destinationPath);
              cout << "File moved: " << sourcePath << " to " << destinationPath << endl;
          } catch (fs::filesystem_error&) {
              // If fs::rename fails, it may be because source and destination are on different filesystems.
              // In that case, use fs::copy to copy the file to the destination and then delete the source.
              fs::copy(sourcePath, destinationPath);
              fs::remove(sourcePath);
              cout << "File moved: " << sourcePath << " to " << destinationPath << endl;
          }
      } else {
          cout << "Source file not found: " << sourcePath << endl;
      }
  }



  void copyFile(const string& source, const string& destination) {
   fs::copy_file(currentPath / source, currentPath / destination, fs::copy_options::overwrite_existing);
   cout << "File copied: " << currentPath / source << " to " << currentPath / destination << endl;
  }

  void encryptFile(const string& fileName, const string& username) {
   if (usersKeys.find(username) != usersKeys.end()) {
       string filePath = (currentPath / fileName).string();
       string content = readFile(filePath);
       string encryptedContent = encrypt(content, usersKeys[username]);
       writeFile(filePath, encryptedContent, true); // Mark file as encrypted by changing its extension
       cout << "File encrypted: " << filePath << endl;
   } else {
       cout << "User not authorized to encrypt files." << endl;
   }

  }

 void decryptFile(const string& fileName, const string& username) {
   if (usersKeys.find(username) != usersKeys.end()) {
       string filePath = (currentPath / fileName).string();
       string content = readFile(filePath);
       string decryptedContent = decrypt(content, usersKeys[username]);
       writeFile(filePath, decryptedContent, false); // Mark file as decrypted by restoring its original extension
       cout << "File decrypted: " << filePath << endl;
   } else {
       cout << "User not authorized to decrypt files." << endl;
   }
  }


  void editFile(const string& fileName) {

          string filePath = (currentPath / fileName).string();
          string textEditor = "nano";  // Change this to your preferred text editor

          // Launch the text editor to edit the file
          string command = textEditor + " " + filePath;
          int result = system(command.c_str());

          if (result == 0) {
              cout << "File edited: " << filePath << endl;
          } else {
              cout << "Error editing the file." << endl;
          }
  }

  string readFile(const string& fileName) {
   fs::path filePath = currentPath / fileName;
   ifstream file(filePath);
   string content;
   if (file.is_open()) {
       stringstream buffer;
       buffer << file.rdbuf();
       content = buffer.str();
       file.close();
       if (isEncrypted(filePath)) {
           return decrypt(content, key);
       }
   }
   return content;
  }

  void deleteFile(const string& fileName) {
   fs::remove(currentPath / fileName);
   cout << "File deleted: " << currentPath / fileName << endl;
  }

  void changeDirectory(const string& path) {
   if (fs::exists(currentPath / path) && fs::is_directory(currentPath / path)) {
       currentPath /= path;
       cout << "Current directory: " << currentPath << endl;
   } else {
       cout << "Invalid directory." << endl;
   }
  }
  void encryptDirectory(const string& dirName, const string& username) {
          string dirPath = (currentPath / dirName).string();
          string encryptedDirPath = dirPath + ".gpg";
          string command = "gpg --symmetric --cipher-algo AES256 --passphrase " + password +
                           " --output " + encryptedDirPath + " " + dirPath;
          int result = system(command.c_str());

          if (result == 0) {
              fs::remove_all(dirPath);
              cout << "Directory encrypted: " << dirPath << " to " << encryptedDirPath << endl;
          } else {
              cout << "Error encrypting directory." << endl;
          }
      }

  void decryptDirectory(const string& dirName, const string& username) {

      string encryptedDirPath = (currentPath / dirName).string();
        string decryptedDirPath = encryptedDirPath.substr(0, encryptedDirPath.size() - 4);  // Remove ".gpg"

        // Reload the GPG agent
        int reloadAgentResult = system("gpg-connect-agent reloadagent /bye");

        if (reloadAgentResult == 0) {
            // Decrypt the directory
            string command = "gpg --batch --yes --decrypt --output " + decryptedDirPath + " " + encryptedDirPath;
            int result = system(command.c_str());

            if (result == 0) {
                fs::remove(encryptedDirPath);
                cout << "Directory decrypted: " << encryptedDirPath << " to " << decryptedDirPath << endl;
            } else {
                cout << "Error decrypting directory." << endl;
            }
        } else {
            cout << "Error reloading GPG agent." << endl;
        }
      }

  void createHash(string fileName){
      string s = "sha512sum -b " +fileName;
      system(s.c_str());
      FILE* pipe = popen(s.c_str(),"r");
      char hash[250];
      fscanf(pipe,"%s",&hash);
      //cout << "In code hash: " << hash <<endl;
      bool found = 0;
      for(auto x: hashTable){
          if(x.first==fileName && x.second == hash){
              cout << "Hash has not been changed" << endl;
              found = 1;
              break;
          }
      }

      if(!found){
          cout << "The file has been altered" <<endl;
          hashTable[fileName] = hash;
      }

  }

private:
  fs::path currentPath;
  string password;
  static string key;
  map<string, string> hashTable;
  map<string, string> usersKeys; // Username -> Encryption Key mapping

  string encrypt(const string& data, const string& encryptionKey) {
         string encryptedData = data;
         for (size_t i = 0; i < data.size(); ++i) {
             encryptedData[i] ^= encryptionKey[i % encryptionKey.size()];
         }
         return encryptedData;
     }

     string decrypt(const string& encryptedData, const string& encryptionKey) {
         return encrypt(encryptedData, encryptionKey); // XOR encryption is symmetric
     }

  bool isEncrypted(const string& filePath) {
   return fs::path(filePath).extension() == ".enc";
  }

  void writeFile(const string& filePath, const string& content, bool encrypted) {
   string finalFilePath = encrypted ? (filePath + ".enc") : filePath;
   ofstream file(finalFilePath);
   if (file.is_open()) {
       file << content;
       file.close();
   }
  }
};
string FileManager::key = "";

int main(){
//QTextStream qtin(stdin);
 string username;
 cout << "Enter your username: ";
 getline(cin,username);
 FileManager fileManager(username);
 cout << "Welcome to the Secure File Manager!" << endl;
 string userInput;
 //cin.ignore();
 while (true) {
     cout << "\nAvailable Commands:" << endl;
     cout << "1. List Files (ls)" << endl;
     cout << "2. Create Directory (mkdir)" << endl;
     cout << "3. Move File (mv)" << endl;
     cout << "4. Copy File (cp)" << endl;
     cout << "5. Encrypt File (encrypt)" << endl;
     cout << "6. Decrypt File (decrypt)" << endl;
     cout << "7. Edit File (edit)" << endl;
     cout << "8. Read File (cat)" << endl;
     cout << "9. Delete File (rm)" << endl;
     cout << "10. Change Directory (cd)" << endl;
     cout << "11. Create File (mkfile)" <<endl;
     cout << "12. Hash Generation and Check (hash)"<<endl;
     cout << "13. Exit (exit)" << endl;
     cout << "Enter your command: ";
     //cin.ignore();
     getline(cin,userInput);
     //cin.ignore();
     if(userInput.empty()){
         cout << "Invalid input. Please try again. "<<endl;
     } else if (userInput == "ls" or userInput == "1") {                                	//LS WORKS
         fileManager.listFiles();

     } else if (userInput == "mkdir" or userInput == "2") {                             	//MKDIR WORKS
         string dirName;
         cout << "Enter directory name: ";
         getline(cin,dirName);
         cin.ignore();
         fileManager.createDirectory(dirName);
     } else if (userInput == "mv" or userInput == "3") {                               	//MV DOES NOT WORK --> only works in the same directory
         string source, destination;
         cout << "Enter source file: ";
         getline(cin,source);
         cin.ignore();
         cout << "Enter destination file: ";
         getline(cin,destination);
        cin.ignore();
         fileManager.moveFile(source, destination);
     } else if (userInput == "cp" or userInput == "4") {                               	//CP WORKS IN THE SAME PATH ONLY
         string source, destination;
         cout << "Enter source file: ";
         getline(cin,source);
         cin.ignore();
         cout << "Enter destination file: ";
         getline(cin,destination);
         cin.ignore();
         fileManager.copyFile(source, destination);

     }
     else if (userInput == "encrypt" or userInput == "5") {
                 string dirName;
                 cout << "Enter directory name to encrypt: ";
                 getline(cin,dirName);
                 cin.ignore();
                 fileManager.encryptDirectory(dirName, username);
                // fileManager.encryptDirectory();


             }
     else if (userInput == "decrypt" or userInput == "6") {
                 string encryptedArchiveName;
                 cout << "Enter encrypted archive name to decrypt: ";
                 getline(cin,encryptedArchiveName);
                 cin.ignore();
                 fileManager.decryptDirectory(encryptedArchiveName, username);
                 //fileManager.decryptDirectory();


     } else if (userInput == "edit" or userInput == "7") {
         string fileName;
         cout << "Enter file name to edit: ";
         getline(cin,fileName);
         cin.ignore();
         fileManager.editFile(fileName);

     } else if (userInput == "cat" or userInput == "8") {                              	//CAT WORKS
         string fileName;
         cout << "Enter file name to read: ";
         getline(cin,fileName);
         cin.ignore();
         string content = fileManager.readFile(fileName);
         cout << "File content:" << endl << content;

     } else if (userInput == "rm" or userInput == "9") {                               	//RM WORKS
         string fileName;
         cout << "Enter file name to delete: ";
         getline(cin,fileName);
         cin.ignore();
         fileManager.deleteFile(fileName);
     } else if (userInput == "cd" or userInput == "10") {                               	//CD WORKS
         string path;
         cout << "Enter directory path: ";
         getline(cin,path);
         cin.ignore();
         fileManager.changeDirectory(path);

     } else if (userInput == "mkfile" or userInput == "11"){                            	//MKFILE WORKS
         string name;
         cout << "Enter file name: ";
         getline(cin,name);
         cin.ignore();
         fileManager.createFile(name);


     } else if (userInput == "hash" or userInput == "12"){
         string fileName;
         cout << "Enter file name: ";
         getline(cin,fileName);
         cin.ignore();
        fileManager.createHash(fileName);
     }
        else if (userInput == "exit" or userInput == "13") {                             	//EXIT WORKS
         cout << "Exiting Secure File Manager. Goodbye!" << endl;
         break;
     } else {
         cout << "Invalid command. Please try again." << endl;
     }
 }

 return 0;
}

