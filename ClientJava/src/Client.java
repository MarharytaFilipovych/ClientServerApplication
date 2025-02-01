import java.io.*;
import java.net.Socket;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

public class Client {
    private static final int CHUNK = 1024;
    final private byte[] buffer = new byte[CHUNK];
    final private Socket socket;
    private final PrintWriter out;
    private final Path database = Paths.get("./database");

    public Client(Socket socket) throws IOException {
        this.socket = socket;
        out = new PrintWriter(socket.getOutputStream(), true);
    }

    private int readBytes()throws IOException{
        return socket.getInputStream().read(buffer);
    }

    private String getResponse(int bytesRead) throws IOException {
        return new String(buffer, 0, bytesRead);
    }

    public void outputServerResponse(){
        try{
            int bytesRead = readBytes();
            if(bytesRead > 0)System.out.println(getResponse(bytesRead));
            else System.out.println("No response from server!");
        }catch(IOException e){
            System.out.println("Error reading server response: " + e.getMessage());
        }
    }

    public void get(Path filePath, int size){
        Path fullPath = database.resolve(filePath.getFileName());
        try(BufferedOutputStream bufferedOutputStream = new BufferedOutputStream(new FileOutputStream(fullPath.toFile()), CHUNK)){
            int i = 0;
            while(i < size){
                 int bytesReceived = readBytes();
                bufferedOutputStream.write(buffer, 0, bytesReceived);
                i+=bytesReceived;
            }
            System.out.println( "File received.");
        }catch(IOException e){
            System.out.println("An error occurred while receiving the file: " + e.getMessage());
        }
    }

    public void put(Path filePath){
        try(BufferedInputStream bufferedInputStream = new BufferedInputStream(new FileInputStream( filePath.toFile()), CHUNK)) {
            int bytesRead;
            byte[] bufferForContent = new byte[CHUNK];
            while((bytesRead = bufferedInputStream.read(bufferForContent))!=-1){
                socket.getOutputStream().write(bufferForContent, 0, bytesRead);
            }
            socket.getOutputStream().flush();
            outputServerResponse();
        } catch (IOException e) {
            System.out.println("An error occurred while file sending.");
        }
    }

    private Path getFile(String filePart){
        String file;
        if(filePart.charAt(0)=='"' && filePart.charAt(filePart.length()-1)=='"') file=filePart.substring(1, filePart.length()-1);
        else file = filePart;
        return Paths.get(file.replace('\\', '/'));
    }

    public void sendMessage(String message){
        out.print(message);
        out.flush();
    }

    public void processRequest(String userInput){
        String[] parts = userInput.split(" ", 2);
        String command = parts[0];
        switch (command){
            case "LIST":
            case "INFO":
            case "DELETE":
                sendMessage(userInput);
                outputServerResponse();
                break;
            case "PUT":
                Path file = getFile(parts[1]);
                try{
                    if(!Files.exists(file) || !Files.isRegularFile(file) || Files.size(file)==0){
                        System.out.println("Something wrong with the file!");
                        return;
                    }
                    sendMessage(userInput + " " + Files.size(file));
                    put(file);
                } catch (IOException e) {
                    System.out.println("Error processing PUT request: " + e.getMessage());
                    return;
                }
                break;
            case "GET":
                sendMessage(userInput);
                try{
                String response = getResponse(readBytes()).trim();
                if(response.equals("Request denied.")) {
                    System.out.println(response);
                    return;
                }
                int size=Integer.parseInt(response.trim());
                get(getFile(parts[1]), size);
                } catch(IOException e){
                    System.out.println("Error processing GET request: " + e.getMessage());
                    return;
                }
                break;
        }
    }
}
