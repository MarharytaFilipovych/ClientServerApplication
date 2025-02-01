import java.util.Arrays;

public enum Request {
    PUT,
    GET,
    DELETE,
    INFO,
    LIST;

    public static boolean isInvalidRequest(String userInput){
        if(userInput.isBlank())return false;
        String command = userInput.split(" ")[0];
        return Arrays.stream(Request.values())
                .noneMatch(request->request.name().equalsIgnoreCase(command))
                || userInput.length()<=command.length()+1;
    }

}
